#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include "freq_list.h"
#include "worker.h"

const FreqRecord record_sentinel = {0, ""};

/**
 * A panic_malloc that panics and quits on on ENOMEM 
 */
void *panic_malloc(size_t size)
{
    void *ptr;
    if ((ptr = malloc(size)) == NULL)
    {
        perror("malloc");
        exit(1);
    }
    return ptr;
}

/**
 * A panic_realloc that panics and quits on on ENOMEM 
 */
void *panic_realloc(void *__ptr, size_t size)
{
    void *ptr;
    if ((ptr = realloc(__ptr, size)) == NULL)
    {
        perror("realloc");
        exit(1);
    }
    return ptr;
}

/* Complete this function for Task 1. Including fixing this comment.
* Gets an array of FreqRecords for the given word. 
*/
FreqRecord *get_word(char *word, Node *head, char **file_names)
{
    // Search for the word in the record
    while (head)
    {
        // if we break
        if (!strncmp(head->word, word, strlen(head->word)))
        {
            break;
        }

        head = head->next;
    }

    // return an empty sentinel
    if (!head)
    {
        FreqRecord *returnRecord = panic_malloc(sizeof(FreqRecord));
        memcpy(returnRecord, &record_sentinel, sizeof(FreqRecord));
        return returnRecord;
    }

    FreqRecord *returnRecord = panic_malloc(sizeof(FreqRecord));
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
        returnRecord = panic_realloc(returnRecord, sizeof(FreqRecord) * (recordCount + 1));
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

    char *listfile = panic_malloc(sizeof(char) * (strlen(dirname) + strlen("/index") + 0x20));
    char *namefile = panic_malloc(sizeof(char) * (strlen(dirname) + strlen("/filenames") + 0x20));

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

    return;
}

typedef struct master_s
{
    // Buffer one record for shifting
    FreqRecord records[MAXRECORDS + 1];
    int count;
} master_s;

MasterArray *ma_init()
{
    MasterArray *arr = panic_malloc(sizeof(master_s));
    arr->count = 0;

    // clear all freq records.
    memset(arr->records, 0, sizeof(FreqRecord) * MAXRECORDS);
    return arr;
}

void ma_insert_record(MasterArray *arr, FreqRecord *frp)
{

    DEBUG_PRINTF("inserting %s\n", frp->filename);

    // nothing to do here.
    if (arr->count == 0)
    {
        DEBUG_PRINTF("count 0\n");

        arr->records[0] = *frp;
        arr->count++;
    }
    // arr has less than MAXRECORDS records => at least one space remaining.
    // arr at least one,
    else if (arr->count < MAXRECORDS)
    {
        DEBUG_PRINTF("count not filled yet. %d items in array\n", arr->count);
        for (int i = 0; i < MAXRECORDS; i++)
        {
            if (arr->records[i].freq >= frp->freq)
                continue;

            DEBUG_PRINTF("shift: Stopped at %d\n", i);
            // arr->records[i].freq < frp->freq <= arr->records[i - 1].freq

            memmove(&arr->records[i + 1], &arr->records[i], sizeof(FreqRecord) * (MAXRECORDS - i - 1));
            arr->records[i] = *frp;
            arr->count++;
            break;
        }
    }
    else
    // arr->count == MAXREECORDS
    {
        for (int i = 0; i < arr->count; i++)
        {
            if (arr->records[i].freq >= frp->freq)
                continue;
            DEBUG_PRINTF("full: Stopped at %d\n", i);

            arr->records[i] = *frp;
            break;
        }
    }
}

void ma_clear(MasterArray *arr)
{
    arr->count = 0;
    memset(arr->records, 0, sizeof(arr->records));
}

FreqRecord *ma_get_record(MasterArray *arr, int i)
{
    return &arr->records[i];
}

void ma_print_array(MasterArray *arr)
{
    print_freq_records(arr->records);
}

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
    nfds_t nfds;
    struct pollfd fds[];
} workerpoll_s;

Worker *worker_create(const char *path)
{
    if (strlen(path) >= 128)
    {
        perror("worker_create: dirname too long");
    }

    Worker *w = panic_malloc(sizeof(Worker));
    int send_pipefd[2];
    int recv_pipefd[2];

    // should be safe from strlen check before.
    memset(w->path, 0, 128);
    strcpy(w->path, path);

    // create pipes

    if (pipe(send_pipefd) == -1 || pipe(recv_pipefd) == -1)
    {
        perror("pipe");
        exit(1);
    }

    w->fd_send_read = send_pipefd[0];
    w->fd_send_write = send_pipefd[1];

    w->fd_recv_read = recv_pipefd[0];
    w->fd_recv_write = recv_pipefd[1];

    return w;
}


/**
 * Closes the worker for send writes on this process.
 * Once closed, the underlying pipe can never be reopened.
 * 
 * In general, this method should not be called directly if
 * working with the worker_* APIs.
 * 
 * Instead, call worker_free once it is certain that the worker
 * will never be reused within the memory space.
 */
void worker_close_send_write(Worker *w)
{
    if (w->fd_send_write == -1)
        return;
    close(w->fd_send_write);
    w->fd_send_write = -1;
}


/**
 * Closes the worker for send reads on this process.
 * Once closed, the underlying pipe can never be reopened.
 * 
 * In general, this method should not be called directly if
 * working with the worker_* APIs.
 * 
 * Instead, call worker_free once it is certain that the worker
 * will never be reused within the memory space.
 */
void worker_close_send_read(Worker *w)
{
    if (w->fd_send_read == -1)
        return;
    close(w->fd_send_read);
    w->fd_send_read = -1;
}


/**
 * Closes the worker for recv writes on this process.
 * Once closed, the underlying pipe can never be reopened.
 * 
 * In general, this method should not be called directly if
 * working with the worker_* APIs.
 * 
 * Instead, call worker_free once it is certain that the worker
 * will never be reused within the memory space.
 */
void worker_close_recv_write(Worker *w)
{
    if (w->fd_recv_write == -1)
        return;
    close(w->fd_recv_write);
    w->fd_recv_write = -1;
}

/**
 * Closes the worker for recv reads on this process.
 * Once closed, the underlying pipe can never be reopened.
 * 
 * In general, this method should not be called directly if
 * working with the worker_* APIs.
 * 
 * Instead, call worker_free once it is certain that the worker
 * will never be reused within the memory space.
 */
void worker_close_recv_read(Worker *w)
{
    if (w->fd_recv_read == -1)
        return;
    close(w->fd_recv_read);
    w->fd_recv_read = -1;
}

ssize_t worker_send(Worker *w, const char *word)
{
    if (w->fd_send_write == -1)
    {
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
    if (w->fd_recv_read == -1)
    {
        perror("worker_recv: recv read closed :(\n");
        return 1;
    }
    memset(frp, 0, sizeof(FreqRecord));
    return read(w->fd_recv_read, frp, sizeof(FreqRecord));
}

WorkerPoll *workerp_create_poll(Worker **ws, int n)
{
    WorkerPoll *poll = panic_malloc(sizeof(WorkerPoll) + n * sizeof(struct pollfd));

    poll->nfds = n;

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

    if (pid == -1)
    {
        perror("fork: unable to create children.");
        exit(1);
    }
    if (pid != 0)
    {
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

int workerp_poll(WorkerPoll *p)
{
    return poll(p->fds, p->nfds, 500);
}

int workerp_check_after_poll(const WorkerPoll *p, int i)
{
    if (p->fds[i].revents & POLLIN)
    {
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
    if (frp == NULL)
        return 1;
    return (frp->freq == 0 && strlen(frp->filename) == 0);
}