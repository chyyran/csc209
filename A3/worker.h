#ifndef WORKER_H
#define WORKER_H

#define PATHLENGTH 128
#define MAXRECORDS 100

#define MAXWORKERS 10

#include <sys/select.h>

// This data structure is used by the workers to prepare the output
// to be sent to the master process.

typedef struct {
    int freq;
    char filename[PATHLENGTH];
} FreqRecord;

FreqRecord *get_word(char *word, Node *head, char **file_names);
void print_freq_records(FreqRecord *frp);
void run_worker(char *dirname, int in, int out);


// --- Master Array APIs
/**
 * Opaque type for MasterArray
 */
typedef struct master_s MasterArray;

/**
 * Instantiates a master array on the heap.
 */
MasterArray *instantiate_master_array();

/**
 * Inserts a new FreqRecord into the MasterArray following the given conditions
 * 
 * - After insertion, the MasterArray is sorted.
 * - If the array is full it will replace the least frequent record.
 */ 
void insert_record(MasterArray *array, FreqRecord *frp);

/**
 * Retrieves a pointer to the record in the MasterArray 
 */
FreqRecord *get_record(MasterArray *array, int i);

// --- Worker APIs

/**
 * Opaque type for Worker.
 */
typedef struct worker_s Worker;

/**
 * Creates a heap-allocated worker for the given directory.
 * 
 * Workers are not executed until called on by worker_run,
 * and read/write pipes are automatically opened for them.
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
 * process/thread. 
 * 
 * Use the worker_close_* APIs to properly close the correct pipe on the 
 * calling process.
 * 
 * Within the spawned process where the run loop executes,
 * the worker will be closed for writes.
 * 
 * To stop this worker, send an EOF or close the underlying write pipe
 * by calling worker_close_write.
 */
void worker_run(const Worker *w);

/**
 * Frees the worker, releasing all memory and file descriptors.
 */
void worker_free(Worker *w);

/**
 * Closes the worker for writes on this process.
 * Once closed, the underlying pipe can never be reopened.
 */
void worker_close_write(Worker *w);

/**
 * Closes the worker for reads on this process.
 * Once closed, the underlying pipe can never be reopened.
 */
void worker_close_read(Worker *w);

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
 * worker_close_write, this method returns 0.
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
 * worker_close_read this method returns 0.
 * 
 * To avoid confusion, this method is intentionally named
 * worker_recv instead of worker_read to indicate that
 * it can only send words.
 */
ssize_t worker_recv(const Worker *w, FreqRecord *record);

#endif /* WORKER_H */
