#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>
#include "freq_list.h"
#include "worker.h"

const FreqRecord record_sentinel = {0, ""};

// -- Utility APIs

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

/**
 * Checks if the given FreqRecord is a sentinel value
 * with frequency 0 and an empty filename.
 */
int is_sentinel(FreqRecord *frp)
{
    if (frp == NULL)
        return 1;
    return (frp->freq == 0 && strlen(frp->filename) == 0);
}

// FreqRecord APIs

/**
 * Retrives the frequency of the given word in the provided filename
 * and indices. If the word is not found, returns a record with
 * frequency 0 and an empty filename.
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

/**
 * Pretty-prints the frequency records for the provided FreqRecord
 * array.
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

/**
 * Reads from the in file descriptor for a given word,
 * searches for it on the index for the given directory, then
 * writes the result to the out file descriptor.
 */
void run_worker(char *dirname, int in, int out)
{

    // some padding bytes for safety
    // 32 seems like a good number.
    
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

// --- Master Array APIs

/**
 * Struct definition for opaque type MasterArray
 * 
 * Used by the main program to collect search results.
 * Use the ma_* APIs to manipulate this array. 
 */
typedef struct master_s
{
    FreqRecord records[MAXRECORDS];
} master_s;

/**
 * Instantiates a master array on the heap.
 */
MasterArray *ma_init()
{
    MasterArray *arr = panic_malloc(sizeof(master_s));

    // clear all freq records.
    memset(arr->records, 0, sizeof(FreqRecord) * MAXRECORDS);
    return arr;
}

/**
 * Inserts a new FreqRecord into the MasterArray following the given conditions
 * 
 * - After insertion, the MasterArray is sorted.
 * - If the array is full it will replace the least frequent record.
 */
void ma_insert_record(MasterArray *arr, FreqRecord *frp)
{

    int next = 0;
    while (arr->records[next].freq >= frp->freq && next < MAXRECORDS - 1)
    {
        next++;
    }
    
    DEBUG_PRINTF("shift: Stopped at %d with values %d compared to %d\n", next, arr->records[next].freq, frp->freq);

    if (arr->records[next].freq > frp->freq)
        return;

    memmove(&arr->records[next + 1], &arr->records[next], sizeof(FreqRecord) * (MAXRECORDS - next - 1));
    arr->records[next] = *frp;
}

/**
 * Clears the master array and deletes all its elements.
 */
void ma_clear(MasterArray *arr)
{
    memset(arr->records, 0, sizeof(arr->records));
}

/**
 * Prints the contents of the master array using print_freq_records
 */
void ma_print_array(MasterArray *arr)
{
    print_freq_records(arr->records);
}

// -- Worker APIs

/**
 * Struct definition for opaque type Worker.
 * 
 * Represents a worker that will run a search on a different 
 * process.
 * 
 * Use the worker_* APIs to manipulate individual Workers.
 */
typedef struct worker_s
{
    // File descriptors for send and recv pipe ends

    int fd_send_write;
    int fd_send_read;

    int fd_recv_write;
    int fd_recv_read;

    // The folder that the worker will search on
    // should contain an index and filenames file.
    char path[128];

    // buffer used for sending messages to this worker
    char sendbuf[32];

} worker_s;

/**
 * Struct definition for Opaque type WorkerPoll.
 * 
 * Represents a pollable collection of running workers.
 * 
 * Use the workerp_* APIs to manipulate pollable collections
 * of Workers.
 */
typedef struct workerpoll_s
{
    // number of file descriptors to poll
    nfds_t nfds;
    // pollfd for each file descriptor
    struct pollfd fds[];
} workerpoll_s;

/**
 * Creates a heap-allocated worker for the given directory.
 * 
 * Workers are not executed until called on by worker_run,
 * and two pairs of read/write pipes are automatically opened,
 * one pair for receiving (recv) data, and one pair for sending (send) 
 * data to the worker.
 * 
 * The write end of the recv pipe is never accessible to the caller,
 * nor is the read end of the send pipe. However, these pipes
 * will remain open until the worker is freed.
 * 
 * In any case, the opened pipes are not intended for direct usage.
 * Instead, data can be sent and received through the worker_send
 * and worker_recv APIs.
 * 
 * Workers own and manage their own resources, including the 
 * opened pipes and a small write buffer used for writing data
 * to the Worker. Such owned members should never be accessed 
 * directly.
 * 
 * Instead, Workers should be manipulated only by the worker_* APIs
 * in worker.h to prevent any resource leaks.
 * 
 * Remember to always free this worker after use with
 * worker_free.
 */
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

/**
 * Send a word to the given worker.
 * 
 * This method returns the result of the underlying
 * write call that writes to the input pipe created for
 * the given worker by worker_create: the number of bytes
 * written to the pipe.
 * 
 * If the word written is greater than 31 characters, it 
 * will be truncated. This method mutates the worker
 * due to reusing an underlying buffer for the write.
 * 
 * If the pipe has been closed previously,
 * this method returns 0.
 */
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

/**
 * Waits and receives for a FreqRecord from this worker.
 *
 * This method returns the result of the underlying
 * read call that reads from the output pipe created for
 * the given worker by worker_create: the number of bytes
 * read from the pipe
 * 
 * If the pipe has been closed previously,
 * this method returns 0.
 */
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

/**
 * Asynchronously begins the run loop for this worker. The worker will 
 * have its pipes remained open for write and read from the calling 
 * process/thread, except for those being used by the spawned process,
 * which will be closed in the calling process.
 * 
 * Because this method closes pipes, it is imperative that a 
 * WorkerPoll is created for this Worker before starting
 * the loop.
 * 
 * Within the spawned process where the run loop executes,
 * the worker will be closed for writes.
 * 
 * To stop this worker, send an EOF or close the underlying write pipe
 * by calling worker_free
 * 
 * This method returns the PID of the spawned process that the loop is
 * running on.
 * 
 * This method will never return on the spawned process, and instead
 * will exit early.
 */
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

/**
 * Frees the worker, releasing all memory and file descriptors.
 */
void worker_free(Worker *w)
{
    // close opened pipes
    worker_close_send_read(w);
    worker_close_send_write(w);

    worker_close_recv_read(w);
    worker_close_recv_write(w);

    free(w);
}

/**
 * Creates a worker poll, parallel to the provided worker array.
 * 
 * Only start Workers with worker_start_run after creating a 
 * WorkerPoll with the Worker, otherwise it may not be
 * properly pollable due to pipes being closed.
 * 
 * A WorkerPoll does not own any resources besides memory and
 * is intended to have a static lifetime once created.
 * 
 * However, it may be safely freed with free().
 */
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

/**
 * Poll the given WorkerPoll for available recv reads.
 * 
 * Ensure that all Workers on this WorkerPoll are running
 * with worker_start_run before attempting to poll.
 * 
 * When data is ready to be read from a given worker, this method
 * will return and modify the given WorkerPoll with 
 * new status.
 *
 * The polling timeout is set to 500ms; this function will
 * return before or after 500ms.
 * 
 * This method returns the same value as the underlying
 * poll call.
 * 
 * After this method returns, call worker_check_after_poll
 * to determine the new status of the workers.
 */
int workerp_poll(WorkerPoll *p)
{
    return poll(p->fds, p->nfds, 500);
}

/**
 * Checks the status of workers after polled.
 * Returns:
 *  0 if reading from this worker will not block (perhaps data is available).
 *  1 if reading from this worker will block (no data available).
 *  -1 if reading from the worker errored.
 * 
 * This method relies on the parallel between the array of Workers this
 * WorkerPoll was created for. If the Worker array changed between then and
 * the call of this function, results will be unexpected.
 */
int workerp_check_after_poll(const WorkerPoll *p, int i)
{
    if (p->fds[i].revents & POLLIN)
    {
        return 0;
    }

    if (p->fds[i].revents & POLLERR)
    {
        return -1;
    }

    return 1;
}
