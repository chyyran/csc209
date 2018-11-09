#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "freq_list.h"
#include "worker.h"
#include <fcntl.h>
#include <errno.h>

/* A program to model calling run_worker and to test it. Notice that run_worker
 * produces binary output, so the output from this program to STDOUT will 
 * not be human readable.  You will need to work out how to save it and view 
 * it (or process it) so that you can confirm that your run_worker 
 * is working properly.
 */
int main(int argc, char **argv)
{
    char ch;
    char path[PATHLENGTH];
    char *startdir = ".";

    /* this models using getopt to process command-line flags and arguments */
    while ((ch = getopt(argc, argv, "d:")) != -1)
    {
        switch (ch)
        {
        case 'd':
            startdir = optarg;
            break;
        default:
            fprintf(stderr, "Usage: queryone [-d DIRECTORY_NAME]\n");
            exit(1);
        }
    }

    // Open the directory provided by the user (or current working directory)
    DIR *dirp;
    if ((dirp = opendir(startdir)) == NULL)
    {
        perror("opendir");
        exit(1);
    }

    /* For each entry in the directory, eliminate . and .., and check
     * to make sure that the entry is a directory, then call run_worker
     * to process the index file contained in the directory.
     * Note that this implementation of the query engine iterates
     * sequentially through the directories, and will expect to read
     * a word from standard input for each index it checks.
     */
    struct dirent *dp;

    Worker **workers = malloc(sizeof(Worker *));

    int nworkers = 0;
    while ((dp = readdir(dirp)) != NULL)
    {
        // do worker instantiatio here.
        if (strcmp(dp->d_name, ".") == 0 ||
            strcmp(dp->d_name, "..") == 0 ||
            strcmp(dp->d_name, ".svn") == 0 ||
            strcmp(dp->d_name, ".git") == 0)
        {
            continue;
        }

        strncpy(path, startdir, PATHLENGTH);
        strncat(path, "/", PATHLENGTH - strlen(path));
        strncat(path, dp->d_name, PATHLENGTH - strlen(path));
        path[PATHLENGTH - 1] = '\0';

        struct stat sbuf;
        if (stat(path, &sbuf) == -1)
        {
            // This should only fail if we got the path wrong
            // or we don't have permissions on this entry.
            perror("stat");
            exit(1);
        }

        // Only call run_worker if it is a directory
        // Otherwise ignore it.
        if (S_ISDIR(sbuf.st_mode))
        {
            // create workers...
            workers = realloc(workers, sizeof(Worker *) * (nworkers + 1));
            workers[nworkers] = worker_create(path);
            nworkers++;
            // run_worker(path, STDIN_FILENO, STDOUT_FILENO);
        }
    }

    if (closedir(dirp) < 0)
        perror("closedir");

    MasterArray *master = instantiate_master_array();

    WorkerPoll *poll = worker_create_poll(workers, nworkers);

    // Start workers
    for (int i = 0; i < nworkers; i++)
    {
        worker_start_run(workers[i]);
    }

    int pipefd_sentinel[2];
    pipe(pipefd_sentinel);
    fcntl(pipefd_sentinel[0], F_SETFL, O_NONBLOCK);

    int pid = fork();
    if (pid == 0)
    {
        char buf[32];
        char sentinel_value = 1;
        int readbytes;
        close(pipefd_sentinel[0]);
        DEBUG_PRINTF("from sentinel process\n");
        while ((readbytes = read(STDIN_FILENO, buf, MAXWORD)) <= MAXWORD && readbytes > 0)
        {
            DEBUG_PRINTF("Found data to be sent: %s\n", buf);
            for (int i = 0; i < nworkers; i++)
            {
                worker_send(workers[i], buf);
            }
            memset(buf, 0, 32);
        }

        DEBUG_PRINTF("eof received. freeing workers\n");
        for (int i = 0; i < nworkers; i++)
        {
            worker_free(workers[i]);
        }

        write(pipefd_sentinel[1], &sentinel_value, sizeof(char));
        DEBUG_PRINTF("wrote end signal to main process\n");
        exit(0);
    }

    close(pipefd_sentinel[1]);

    char quit = 0;
    FreqRecord freqbuf;
    while ((read(pipefd_sentinel[0], &quit, sizeof(char))) == -1 && quit == 0)
    {
        int ret = workers_poll(poll);
        for (int i = 0; i < nworkers; i++)
        {
            if (!worker_check_after_poll(poll, i))
            {
                DEBUG_PRINTF("working for worker %d\n", i);
                if (worker_recv(workers[i], &freqbuf) != 0)
                {
                    DEBUG_PRINTF("working for worker recv %d\n", i);
                    if (is_sentinel(&freqbuf))
                    {
                        DEBUG_PRINTF("received sentinel\n");
                        continue;
                    }

                    DEBUG_PRINTF("%s\n", freqbuf.filename);
                }
            }
        }
    }

    // release all pipes
    for (int i = 0; i < nworkers; i++)
    {
        worker_free(workers[i]);
    }

    return 0;
}