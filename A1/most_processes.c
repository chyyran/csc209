#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENTRY_MAX 1024
#define UID_MAX 31

int main(int argc, char **argv)
{
    char buf[ENTRY_MAX];
    char most_uid[UID_MAX];
    int most_proc_count = 0;

    char cur_uid[UID_MAX];
    int proc_count = 0;
    int cur_ppid = 0;

    int mask_ppid = -1;

    if (argc == 2)
    {
        mask_ppid = strtol(argv[1], NULL, 10);
    }
    else if (argc > 2)
    {
        printf("USAGE: most_processes [ppid]\n");
        return 1;
    }

    while (fgets(buf, ENTRY_MAX, stdin) != NULL)
    {
        char loop_uid[UID_MAX];
        if (sscanf(buf, "%s\t%*d\t%d[^\n]", loop_uid, &cur_ppid) == 2)
        {
            //printf("%s: %d\n", loop_uid, cur_ppid);
            // consider ppid here.
            if (mask_ppid > -1 && mask_ppid != cur_ppid)
                continue;

            if (strcmp(cur_uid, loop_uid) == 0)
            {
                // Still on the same UID.
                proc_count++;
            }
            else
            {
                // Different UID
                if (proc_count > most_proc_count)
                {
                    // Save the most proc count if bigger.
                    strcpy(most_uid, cur_uid);
                    most_proc_count = proc_count;
                }
                strcpy(cur_uid, loop_uid);
                proc_count = 1;
            }
        }
    }

    // Need to account for last entries.
    if (proc_count > most_proc_count)
    {
        // Save the most proc count if bigger.
        strcpy(most_uid, cur_uid);
        most_proc_count = proc_count;
    }

    if (most_proc_count > 0)
    {
        printf("%s %d", most_uid, most_proc_count);
    }

    return 0;
}