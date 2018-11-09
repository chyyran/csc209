#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include "freq_list.h"
#include "worker.h"

const FreqRecord record_sentinel = {0, ""};

/* Complete this function for Task 1. Including fixing this comment.
* Gets an array of FreqRecords for the given word. 
*/
FreqRecord *get_word(char *word, Node *head, char **file_names)
{
    // Search for the word in the record
    DEBUG_PRINTF("searching for %s\n", word);
    while (head)
    {
        // if we break
        if (!strncmp(head->word, word, strlen(head->word))) {
            DEBUG_PRINTF("word found!: %s\n", head->word);
            break;
        }
            
        DEBUG_PRINTF("%s != %s\n", head->word, word);
        head = head->next;
    }

    // return an empty sentinel
    if (!head)
    {
        DEBUG_PRINTF("none found\n");
        FreqRecord *returnRecord = malloc(sizeof(FreqRecord));
        memcpy(returnRecord, &record_sentinel, sizeof(FreqRecord));
        return returnRecord;
    }

    FreqRecord *returnRecord = malloc(sizeof(FreqRecord));
    int recordCount = 0;
    for (int i = 0; i < MAXFILES; i++)
    {

        // skip any zero frequency entries...
        // this will also ensure that file_names[i] is a valid pointer
        // since any entries past sizeof(file_names) / MAXFILES in freq will be 0.
        // from parallel arrays.
        if (head->freq[i] <= 0)
            continue;

        FreqRecord f = {.freq = head->freq[i]};
        strcpy(f.filename, file_names[i]);

        // copy record to the heap
        memcpy(&returnRecord[recordCount], &f, sizeof(FreqRecord));
        recordCount++;

        // allocate enough space for the next record.
        returnRecord = realloc(returnRecord, sizeof(FreqRecord) * (recordCount + 1));
    }
    memcpy(&returnRecord[recordCount], &record_sentinel, sizeof(FreqRecord));
    return returnRecord;
}

/* Print to standard output the frequency records for a word.
* Use this for your own testing and also for query.c
*/
void print_freq_records(FreqRecord *frp)
{
    int i = 0;

    while (frp != NULL && frp[i].freq != 0)
    {
        printf("%d    %s\n", frp[i].freq, frp[i].filename);
        i++;
    }
}

/* Complete this function for Task 2 including writing a better comment.
*/
void run_worker(char *dirname, int in, int out)
{

    char *listfile = malloc(sizeof(char) * (strlen(dirname) + strlen("/index") + 0x20));
    char *namefile = malloc(sizeof(char) * (strlen(dirname) + strlen("/filenames") + 0x20));

    sprintf(listfile, "%s/%s", dirname, "index");
    sprintf(namefile, "%s/%s", dirname, "filenames");

    Node *head = NULL;
    char **filenames = init_filenames();

    read_list(listfile, namefile, &head, filenames);

    int readbytes = 0;
    char buf[MAXWORD];
    memset(buf, 0, MAXWORD);
    DEBUG_PRINTF("from worker thread, in: %d, out: %d\n", in, out);

    while ((readbytes = read(in, buf, MAXWORD)) <= MAXWORD && readbytes > 0)
    {
        DEBUG_PRINTF("from worker thread inside loop: %s\n", buf);
        int i = 0;
        FreqRecord *records = get_word(buf, head, filenames);
        while (records != NULL && records[i].freq != 0)
        {
            write(out, &records[i], sizeof(FreqRecord));
            DEBUG_PRINTF("sent to out %d: %s\n", records[i].freq, records[i].filename);
            i++;
        }

        DEBUG_PRINTF("records gotten finsihed\n");

        memset(buf, 0, MAXWORD); // better safe than sorry, flush the buffer.

        // write the sentinel
        write(out, &record_sentinel, sizeof(FreqRecord));
        free(records);
    }

    DEBUG_PRINTF("all gone! %d in: %d, out: %d buf: %s\n", readbytes, in, out, buf);
    free(listfile);
    free(namefile);
    //todo: free init_filenames
    return;
}

// todo: remove pragmas
#ifndef __GNUC__
#pragma region master
#endif

typedef struct master_s
{
    FreqRecord records[MAXRECORDS];
    int count;
    int next;
} master_s;

MasterArray *ma_init()
{
    MasterArray *arr = malloc(sizeof(master_s));
    arr->count = 0;

    // next is incremented before insertion.
    arr->next = -1;

    // clear all freq records.
    memset(arr->records, 0, sizeof(FreqRecord) * MAXRECORDS);
    return arr;
}

void ma_insert_record(MasterArray *arr, FreqRecord *frp)
{
    // do insertion here...
    // full

    if (arr->count < MAXRECORDS)
    {
        arr->count++;
        // do caculation of arr->next here,
        // for after sort.
        // this should also ensure the array is sorted, shifting the array
        // if necessary...
        // use memmove.
    }
    else
    {
        // find the least, set that index to arr->next
    }

    // note: this is a copy.
    arr->records[arr->next] = *frp;
}

FreqRecord *ma_get_record(MasterArray *arr, int i)
{
    return &arr->records[i];
}

void ma_print_array(MasterArray *arr)
{
    print_freq_records(arr->records);
}

#ifndef __GNUC__
#pragma endregion
#endif

// todo: remove pragmas
#ifndef __GNUC__
#pragma region worker
#endif


typedef struct worker_s
{
    int fd_send_write;
    int fd_send_read;

    int fd_recv_write;
    int fd_recv_read;

    char path[128];
    char sendbuf[32];


} worker_s;

typedef struct workerpoll_s
{
    Worker **workers;
    nfds_t nfds;
    struct pollfd fds[];
} workerpoll_s;


Worker *worker_create(const char *path)
{
    if (strlen(path) >= 128)
    {
        perror("worker_create: dirname too long");
    }

    Worker *w = malloc(sizeof(Worker));
    int send_pipefd[2];
    int recv_pipefd[2];

    // should be safe from strlen check before.
    memset(w->path, 0, 128);
    strcpy(w->path, path);

    // create pipes

    pipe(send_pipefd);
    pipe(recv_pipefd);

    w->fd_send_read = send_pipefd[0];
    w->fd_send_write = send_pipefd[1];

    w->fd_recv_read = recv_pipefd[0];
    w->fd_recv_write = recv_pipefd[1];

    
    return w;
}

void worker_close_send_write(Worker *w)
{
    if (w->fd_send_write == -1)
        return;
    close(w->fd_send_write);
    w->fd_send_write = -1;
}

void worker_close_send_read(Worker *w)
{
    if (w->fd_send_read == -1)
        return;
    close(w->fd_send_read);
    w->fd_send_read = -1;
}

void worker_close_recv_write(Worker *w)
{
    if (w->fd_recv_write == -1)
        return;
    close(w->fd_recv_write);
    w->fd_recv_write = -1;
}

void worker_close_recv_read(Worker *w)
{
    if (w->fd_recv_read == -1)
        return;
    close(w->fd_recv_read);
    w->fd_recv_read = -1;
}

ssize_t worker_send(Worker *w, const char *word)
{
    if (w->fd_send_write == -1) {
        DEBUG_PRINTF("failed to send\n");
        return 0;
    }
    // prepare buffer for a safe send
    // instead of allocating a new buffer on the stack,
    // have one reused for performance and safety reasons.
    memset(w->sendbuf, 0, 32);
    strncpy(w->sendbuf, word, 31);
    w->sendbuf[31] = '\0';

    DEBUG_PRINTF("sending value %s\n", w->sendbuf);
    return write(w->fd_send_write, w->sendbuf, 32);
}

ssize_t worker_recv(const Worker *w, FreqRecord *frp)
{
    if (w->fd_recv_read == -1) {
        DEBUG_PRINTF("recv read closed :(\n");
        return 1;
    }
    memset(frp, 0, sizeof(FreqRecord));
    return read(w->fd_recv_read, frp, sizeof(FreqRecord));
}

WorkerPoll *worker_create_poll(Worker **ws, int n)
{
    WorkerPoll *poll = malloc(sizeof(WorkerPoll) + n * sizeof(struct pollfd));

    poll->nfds = n;
    poll->workers = ws;

    for (int i = 0; i < poll->nfds; i++)
    {
        poll->fds[i].fd = ws[i]->fd_recv_read;
        poll->fds[i].events = POLLIN;
    }
    return poll;
}

int worker_start_run(Worker *w) 
{
    int pid = fork();

    if (pid != 0) { 
        // close unused pipes.
        worker_close_send_read(w);
        worker_close_recv_write(w);
        return pid;
    }

    // -- child process
    DEBUG_PRINTF("starting child process...\n");
    // close unused pipes.
    worker_close_recv_read(w);
    worker_close_send_write(w);
    run_worker(w->path, w->fd_send_read, w->fd_recv_write);
    worker_free(w);
    exit(0);
    return pid;
}

int workers_poll(WorkerPoll *p)
{
    return poll(p->fds, p->nfds, 500);
}

int worker_check_after_poll(const WorkerPoll *p, int i)
{
    if (p->fds[i].revents & POLLIN) 
    {
        DEBUG_PRINTF("worker %d is ready\n", i);
         return 0; 
    }
    return 1;
}

void worker_free(Worker *w)
{
    // close opened pipes
    worker_close_send_read(w);
    worker_close_send_write(w);

    worker_close_recv_read(w);
    worker_close_recv_write(w);

    free(w);
}

int is_sentinel(FreqRecord *frp)
{
    if (frp == NULL) return 1;
    return (frp->freq == 0 && strlen(frp->filename) == 0);
}
#ifndef __GNUC__
#pragma endregion
#endif