#include "concurrency.h"

#include <semaphore.h>

sem_t mutex;

void *thread (void *arg)
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

	sem_wait(&mutex);
	for (a = 0; a < HISTSIZE; a++)
		histogram[a] += local[a];
	sem_post(&mutex);

	return NULL;
}



void concurrent_cg(char *dirname) {

	sem_init(&mutex, 0, 1);
    // try to change to directory specified in args
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
	    pthread_create(&tlist[i], NULL, thread, (void *)(&(fds[i])));
    }
    for (i = 0; i < count; i++)
	    pthread_join(tlist[i], NULL);
    // cleanup
    free(fds);
    // work is done, return
}

