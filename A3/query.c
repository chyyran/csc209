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

    Worker **workers = panic_malloc(sizeof(Worker *));

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

            // Check here to avoid run_worker returning errors
            // despite repeating code.
            char *listfile = panic_malloc(sizeof(char) * (strlen(path) + strlen("/index") + 0x20));
            char *namefile = panic_malloc(sizeof(char) * (strlen(path) + strlen("/filenames") + 0x20));

            sprintf(listfile, "%s/%s", path, "index");
            sprintf(namefile, "%s/%s", path, "filenames");

            if (access(listfile, R_OK) == -1 || access(namefile, R_OK) == -1)
            {
                perror("access: unable to access index or filenames");
                exit(1);
            }

            // create workers...
            workers = panic_realloc(workers, sizeof(Worker *) * (nworkers + 1));
            workers[nworkers] = worker_create(path);
            nworkers++;
        }
    }

    if (closedir(dirp) < 0)
    {
        perror("closedir");
        exit(1);
    }

    // init management objects...
    WorkerPoll *poll = workerp_create_poll(workers, nworkers);

    // Start workers
    for (int i = 0; i < nworkers; i++)
    {
        worker_start_run(workers[i]);
    }

    // create the non-blocking exit signaller
    int pipefd_sentinel[2];
    if (pipe(pipefd_sentinel) == -1)
    {
        perror("pipe");
        exit(1);
    }
    fcntl(pipefd_sentinel[0], F_SETFL, O_NONBLOCK);

    // fork a process to listen to STDIN
    int pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(1);
    }
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

    // main process handles the master array
    MasterArray *master = ma_init();

    char quit = 0;
    FreqRecord freqbuf;
    int workerstat = 1;
    int sentinel_count = 0;
    while ((read(pipefd_sentinel[0], &quit, sizeof(char))) == -1 && quit == 0)
    {

        workerp_poll(poll);
        for (int i = 0; i < nworkers; i++)
        {
            if ((workerstat = workerp_check_after_poll(poll, i)) == 0)
            {
                if (worker_recv(workers[i], &freqbuf) != 0)
                {
                    if (is_sentinel(&freqbuf))
                    {
                        DEBUG_PRINTF("received sentinel\n");
                        sentinel_count++;
                        continue;
                    }
                    ma_insert_record(master, &freqbuf);
                }
            }
            else if (workerstat == -1)
            {
                perror("poll: worker failed");
            }
        }

        if (sentinel_count >= nworkers) {
            sentinel_count = 0;
            ma_print_array(master);
            ma_clear(master);
        }
    }

    // release all pipes
    for (int i = 0; i < nworkers; i++)
    {
        worker_free(workers[i]);
    }

    return 0;
}
