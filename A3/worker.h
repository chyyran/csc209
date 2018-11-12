#ifndef WORKER_H
#define WORKER_H

#define PATHLENGTH 128
#define MAXRECORDS 100

#define MAXWORKERS 10

#include <sys/poll.h>

// FreqRecord APIs

/**
 * This data structure is used by the workers to prepare the output
 * to be sent to the master process.
 */
typedef struct
{
    int freq;
    char filename[PATHLENGTH];
} FreqRecord;

/**
 * Retrives the frequency of the given word in the provided filename
 * and indices. If the word is not found, returns a record with
 * frequency 0 and an empty filename.
 */
FreqRecord *get_word(char *word, Node *head, char **file_names);

/**
 * Pretty-prints the frequency records for the provided FreqRecord
 * array.
 */
void print_freq_records(FreqRecord *frp);

/**
 * Reads from the in file descriptor for a given word,
 * searches for it on the index for the given directory, then
 * writes the result to the out file descriptor.
 */
void run_worker(char *dirname, int in, int out);

// -- Utility APIs

/**
 * Checks if the given FreqRecord is a sentinel value
 * with frequency 0 and an empty filename.
 */
int is_sentinel(FreqRecord *frp);

/**
 * A malloc that panics and quits on on ENOMEM 
 */
void *panic_malloc(size_t size);

/**
 * A realloc that panics and quits on on ENOMEM 
 */
void *panic_realloc(void *__ptr, size_t size);

// --- Master Array APIs
/**
 * Opaque type for MasterArray
 * Used by the main program to collect search results.
 * 
 * Use the ma_* APIs to manipulate this array. 
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
 * Clears the master array and deletes all its elements.
 */
void ma_clear(MasterArray *array);

/**
 * Prints the contents of the master array using print_freq_records
 */
void ma_print_array(MasterArray *array);

/**
 * Retrieves a pointer to the record in the MasterArray 
 */
FreqRecord *ma_get_record(MasterArray *array, int i);

// --- Worker APIs

/**
 * Opaque type for Worker.
 * 
 * Represents a worker that will run a search on a different 
 * process.
 * 
 * Use the worker_* APIs to manipulate individual Workers.
 */
typedef struct worker_s Worker;

/**
 * Opaque type for WorkerPoll.
 * 
 * Represents a pollable collection of running workers.
 * 
 * Use the workerp_* APIs to manipulate pollable collections
 * of Workers.
 */
typedef struct workerpoll_s WorkerPoll;

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
Worker *worker_create(const char *dirname);

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
int worker_start_run(Worker *w);

/**
 * Frees the worker, releasing all memory and file descriptors.
 */
void worker_free(Worker *w);

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
ssize_t worker_send(Worker *w, const char *word);

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
ssize_t worker_recv(const Worker *w, FreqRecord *record);

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
WorkerPoll *workerp_create_poll(Worker **ws, int n);

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
int workerp_poll(WorkerPoll *w);

/**
 * Checks the status of workers after polled.
 * Returns:
 *  0 if reading from this worker will not block (perhaps data is available).
 *  1 if reading from this worker will block (no data available).
 * 
 * This method relies on the parallel between the array of Workers this
 * WorkerPoll was created for. If the Worker array changed between then and
 * the call of this function, results will be unexpected.
 */
int workerp_check_after_poll(const WorkerPoll *w, int i);

#ifdef DEBUG
#define DEBUG_PRINTF(...)        \
    do                           \
    {                            \
        if (DEBUG)               \
        {                        \
            printf(__VA_ARGS__); \
        }                        \
    } while (0)
#else
#define DEBUG_PRINTF(fmt, ...)
#endif
#endif /* WORKER_H */
