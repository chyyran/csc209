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
    while (head)
    {
        // if we break
        if (!strcmp(head->word, word))
            break;

        head = head->next;
    }

    // return an empty sentinel
    if (!head)
    {
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

    while ((readbytes = read(in, buf, MAXWORD)) <= MAXWORD && readbytes != 0 && readbytes != EOF)
    {
        int i = 0;
        FreqRecord *records = get_word(buf, head, filenames);
        while (records != NULL && records[i].freq != 0)
        {
            write(out, &records[i], sizeof(FreqRecord));
            printf("\n%d: %s\n", records[i].freq, records[i].filename);
            i++;
        }

        memset(buf, 0, MAXWORD); // better safe than sorry, flush the buffer.

        // write the sentinel
        write(out, &record_sentinel, sizeof(FreqRecord));
        free(records);
    }

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

MasterArray *instantiate_master_array()
{
    MasterArray *arr = malloc(sizeof(master_s));
    arr->count = 0;

    // next is incremented before insertion.
    arr->next = -1;

    // clear all freq records.
    memset(arr->records, 0, sizeof(FreqRecord) * MAXRECORDS);
    return arr;
}

void insert_record(MasterArray *arr, FreqRecord *frp)
{
    // do insertion here...
    // full


    if (arr->count < MAXRECORDS) {
        arr->count++;
        // do caculation of arr->next here, 
        // for after sort.
        // this should also ensure the array is sorted, shifting the array
        // if necessary...
        // use memmove.

    } else {
        // find the least, set that index to arr->next
    }   

    // note: this is a copy.
    arr->records[arr->next] = *frp;
}

FreqRecord *get_record(MasterArray *arr, int i)
{
    return &arr->records[i];
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
    int fd_write;
    int fd_read;

    char path[128];
    char sendbuf[32];
} worker_s;

Worker *worker_create(const char *path)
{
    if (strlen(path) >= 128)
    {
        perror("worker_create: dirname too long");
    }

    Worker *w = malloc(sizeof(worker_s));
    int pipefd[2];

    // should be safe from strlen check before.
    memset(w->path, 0, 128);
    strcpy(w->path, path);

    pipe(pipefd);

    w->fd_write = pipefd[0];
    w->fd_read = pipefd[1];
    return w;
}


void worker_close_write(Worker *w)
{
    if (w->fd_write == -1)
        return;
    close(w->fd_write);
    w->fd_write = -1;
}

void worker_close_read(Worker *w)
{
    if (w->fd_read == -1)
        return;
    close(w->fd_read);
    w->fd_read = -1;
}

ssize_t worker_send(Worker *w, const char *word) 
{
    if (w->fd_write == -1) return 0;
    // prepare buffer for a safe send
    // instead of allocating a new buffer on the stack, 
    // have one reused for performance and safety reasons.
    memset(w->sendbuf, 0, 32);
    strncpy(w->sendbuf, word, 31);
    w->sendbuf[31] = '\0';

    return write(w->fd_write, w->sendbuf, 32);
}

ssize_t worker_recv(const Worker *w, FreqRecord *frp)
{
    if (w->fd_read == -1) return 0;
    return read(w->fd_read, frp, sizeof(FreqRecord));
}

void worker_free(Worker *w)
{
    // close opened pipes
    worker_close_read(w);
    worker_close_write(w);

    free(w);
}

#ifndef __GNUC__
#pragma endregion
#endif