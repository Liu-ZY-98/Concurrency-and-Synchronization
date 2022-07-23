#include "concurrency.h"
#include <semaphore.h>

sem_t mutex, w; 
int cnt = 0;

void snapshot_stats() {
    int i, sum = 0;
    int cfreq[HISTSIZE];
    statsnap s;
    s.n = 0;
    
    // part 2 put your locks here
    sem_wait(&mutex);
    cnt++;
    if (cnt == 1)
	    sem_wait(&w);
    sem_post(&mutex);

    s.mode = -1;
    int maxfreq = -1;
    for (i = 0; i < HISTSIZE; i++) {
        s.n += histogram[i];
        cfreq[i] = s.n;
        sum += i * histogram[i];        
        if (maxfreq < histogram[i]) {
            s.mode = i;
            maxfreq = histogram[i];
        }
    }

    // part 2 put your locks here

    sem_wait(&mutex);
    cnt--;
    if (cnt == 0)
	    sem_post(&w);
    sem_post(&mutex);

    s.mean = calc_mean_median(sum, s.n, cfreq, &s.median);
    // send stats
    Write(statpipe[1], &s, sizeof(statsnap));
}

void snapshot_histogram() {
    int i;
    histsnap s;
    s.n = 0;
    
    // part 2 put your locks here

    sem_wait(&mutex);
    cnt++;
    if (cnt == 1)
	    sem_wait(&w);
    sem_post(&mutex);
    
    for (i = 0; i < HISTSIZE; i++) {
        s.n += histogram[i];
        s.hist[i] = histogram[i];
    }
    
    // part 2 put your locks here
    
    sem_wait(&mutex);
    cnt--;
    if (cnt == 0)
	    sem_post(&w);
    sem_post(&mutex);

    // send n and histogram   
    Write(histpipe[1], &s, sizeof(histsnap));
}

void *readerwriter_stat_task(void *data) {
    while(1) {
        usleep(READSLEEP);
        snapshot_stats();
    }
    return NULL;
}

void *readerwriter_hist_task(void *data) {
    while(1) {
        usleep(READSLEEP);
        snapshot_histogram();
    }
    return NULL;
}

void start_readers(pthread_t *readers) {
    pthread_create(&readers[0], NULL, readerwriter_stat_task, NULL);
    pthread_create(&readers[1], NULL, readerwriter_hist_task, NULL);

}

void *rthread (void *arg)
{
	int fd = *((int *)arg);
	int local[HISTSIZE];

	int a;
	for (a = 0; a < HISTSIZE; a++)
		local[a] = 0;
	
	char c;
	int rv;

        while((rv = Read(fd, &c, 1)) != 0) 
	{
            c -= '0';
            // if the byte is out of range, skip it
            if (c < 0 || c >= HISTSIZE) {
                fprintf(stderr, "skipping %c\n", c);
                continue;
            }
            local[(int)c]++;           
        }
        // cleanup
        Close(fd);

	sem_wait(&w);
	for (a = 0; a < HISTSIZE; a++)
		histogram[a] += local[a];
	sem_post(&w);

	return NULL;
}
void readerwriter(char *dirname) {
    // start reader threads
    pthread_t readers[2];
    start_readers(readers);
    sem_init(&mutex, 0, 1);
    sem_init(&w, 0, 1);

    // your code here

    Chdir(dirname);
    // open directory stream
    DIR *dp = Opendir("./");

    struct dirent *ep;
    // start by assuming there will be only 8 files
    // reallocate if this guess is too small
    int numfds = 8;
    // allocate space for file information
    int *fds = Malloc(numfds*sizeof(int));
    int count = 0;
    while ((ep = readdir(dp)) != NULL) {
        // make sure file exists
        struct stat sb;
        Stat(ep->d_name, &sb);
        // make sure that its a "regular" file (i.e not a directory, link etc.)
        if ((sb.st_mode & S_IFMT) != S_IFREG) {
            continue;
        }
        // open file 
        fds[count] = Open(ep->d_name, O_RDONLY);
        count++;
        // if the number of files is more than what we guessed
        // ask for more space
        if (count == numfds) {
            numfds = numfds * 2;
            fds = Realloc(fds, numfds*sizeof(int));
        }
    }
    // cleanup
    Closedir(dp);

    int i;
    // go through all the files
        pthread_t *tlist = malloc(count * sizeof(pthread_t));
    // go through all the files
    for (i = 0; i < count; i++) 
    {
	    pthread_create(&tlist[i], NULL, rthread, (void *)(&(fds[i])));
    }
    for (i = 0; i < count; i++)
	    pthread_join(tlist[i], NULL);
    // cleanup
    free(fds);
    // work is done, return
    // remember to cancel the reader threads when you are done processing all the files
}
