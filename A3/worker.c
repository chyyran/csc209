#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include "freq_list.h"
#include "worker.h"

/* Complete this function for Task 1. Including fixing this comment.
* Gets an array of FreqRecords for the given word. 
*/
FreqRecord *get_word(char *word, Node *head, char **file_names)
{

    // instantiate sentinel
    FreqRecord sentinel = {0, ""};

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
        memcpy(returnRecord, &sentinel, sizeof(sentinel));
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
    memcpy(&returnRecord[recordCount], &sentinel, sizeof(sentinel));
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
    Node *head = NULL;
    char **filenames = init_filenames();
    char *listfile = malloc(sizeof(char) * (strlen(dirname) + strlen("/index" + 1)));
    char *namefile = malloc(sizeof(char) * (strlen(dirname) + strlen("/filenames" + 1)));

    sprintf(listfile, "%s/%s", dirname, "index");
    sprintf(namefile, "%s/%s", dirname, "filenames");

    read_list(listfile, namefile, &head, filenames);

    int readbytes;
    char buf[32];

    while ((readbytes = read(in, buf, MAXWORD)) == MAXWORD)
    {
        int i = 0;
        FreqRecord sentinel = {0, ""};
        FreqRecord *records = get_word(buf, head, filenames);
        while (records != NULL && records[i].freq != 0) {
            write(out, &records[i], sizeof(FreqRecord));
        }
        write(out, &sentinel, sizeof(FreqRecord));
        free(records);
    }

    free(listfile);
    free(namefile);
    //todo: free init_filenames
    return;
}
