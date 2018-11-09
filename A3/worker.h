#ifndef WORKER_H
#define WORKER_H

#define PATHLENGTH 128
#define MAXRECORDS 100

#define MAXWORKERS 10

#include <sys/poll.h>

// This data structure is used by the workers to prepare the output
// to be sent to the master process.

typedef struct {
    int freq;
    char filename[PATHLENGTH];
} FreqRecord;

FreqRecord *get_word(char *word, Node *head, char **file_names);
void print_freq_records(FreqRecord *frp);
void run_worker(char *dirname, int in, int out);
int is_sentinel(FreqRecord *frp);



/**
 * A malloc that panics and quits on on ENOMEM 
 */
void *panic_malloc(size_t size);

// --- Master Array APIs
/**
 * Opaque type for MasterArray
 */
typedef struct master_s MasterArray;

/**
 * Instantiates a master array on the heap.
 */
MasterArray *ma_init();

/**
 * Inserts a new FreqRecord into the MasterArray following the given conditions
 * 
 * - After insertion, the MasterArray is sorted.
 * - If the array is full it will replace the least frequent record.
 */ 
void ma_insert_record(MasterArray *array, FreqRecord *frp);

/**
 * Prints the master array using 
 */
void ma_print_array(MasterArray *array);
/**
 * Retrieves a pointer to the record in the MasterArray 
 */
FreqRecord *ma_get_record(MasterArray *array, int i);

// --- Worker APIs

/**
 * Opaque type for Worker.
 */
typedef struct worker_s Worker;

typedef struct workerpoll_s WorkerPoll;

/**
 * Handles when a worker has data ready.
 */
typedef void (*WorkerDataReadyHandler)(Worker *w);

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
 * will remain open until the corresponding worker_close_* API
 * is called for each of the 4 pipe ends.
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
Worker *worker_create(const char *dirname);

/**
 * Asynchronously begins the run loop for this worker. The worker will 
 * have its pipes remained open for write and read from the calling 
 * process/thread, except for those being used by the spawned process,
 * which will be closed in the calling process.
 * 
 * Within the spawned process where the run loop executes,
 * the worker will be closed for writes.
 * 
 * To stop this worker, send an EOF or close the underlying write pipe
 * by calling worker_close_write.
 * 
 * This method returns the PID of the spawned process that the loop is
 * running on.
 * 
 * This method will never return on the spawned process, and instead
 * will exit early.
 */
int worker_start_run(Worker *w);

/**
 * Creates a worker poll, parallel to the provided worker array.
 */
WorkerPoll *worker_create_poll(Worker **ws, int n);

/**
 * Poll the given WorkerPoll for available recv reads.
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
int workers_poll(WorkerPoll *w);

/**
 * Checks the status of workers after polled.
 * Returns:
 *  0 if reading from this worker will not block (perhaps data is available).
 *  1 if reading from this worker will block (no data available).
 */
int worker_check_after_poll(const WorkerPoll *w, int i);

/**
 * Frees the worker, releasing all memory and file descriptors.
 */
void worker_free(Worker *w);

/**
 * Closes the worker for send writes on this process.
 * Once closed, the underlying pipe can never be reopened.
 */
void worker_close_send_write(Worker *w);

/**
 * Closes the worker for send reads on this process.
 * Once closed, the underlying pipe can never be reopened.
 */
void worker_close_send_read(Worker *w);

/**
 * Closes the worker for recv writes on this process.
 * Once closed, the underlying pipe can never be reopened.
 */
void worker_close_recv_write(Worker *w);

/**
 * Closes the worker for recv reads on this process.
 * Once closed, the underlying pipe can never be reopened.
 */
void worker_close_recv_read(Worker *w);

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
 * If the pipe has been closed previously by 
 * worker_close_send_write, this method returns 0.
 * 
 * To avoid confusion, this method is intentionally named
 * worker_send instead of worker_write to indicate that
 * it can only send words.
 */
ssize_t worker_send(Worker *w, const char *word);

/**
 * Waits and receives for a FreqRecord from this worker.
 *
 * This method returns the result of the underlying
 * read call that reads from the output pipe created for
 * the given worker by worker_create: the number of bytes
 * read from the pipe
 * 
 * If the pipe has been closed previously by 
 * worker_close_recv_read this method returns 0.
 * 
 * To avoid confusion, this method is intentionally named
 * worker_recv instead of worker_read to indicate that
 * it can only send words.
 */
ssize_t worker_recv(const Worker *w, FreqRecord *record);


#ifdef DEBUG
#define DEBUG_PRINTF(...) do { if (DEBUG) { printf(__VA_ARGS__); } } while (0)
#else
#define DEBUG_PRINTF(fmt, ...)
#endif
#endif /* WORKER_H */
