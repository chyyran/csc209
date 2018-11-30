#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hcq.h"
#include "srvman.h"

#ifndef PORT
#define PORT 30000
#endif

#define MAX_BACKLOG 5
#define INPUT_BUFFER_SIZE 256
#define INPUT_ARG_MAX_NUM 3
#define DELIM " \n"

/* Print a formatted error message to stderr.
 */
int error(char *msg, char **buf)
{
    asprintf(buf, "Error: %s\n", msg);
    return 0;
}

// Use global variables so we can have exactly one TA list and one student list
Ta *ta_list = NULL;
Student *stu_list = NULL;

Course *courses;
int num_courses = 3;

/* 
 * Read and process commands
 * Return:  -1 for quit command
 *          0 otherwise
 */
int process_args(int cmd_argc, char **cmd_argv, char **ptr)
{

    int result;

    if (cmd_argc <= 0)
    {
        return 0;
    }
    else if (strcmp(cmd_argv[0], "add_student") == 0 && cmd_argc == 3)
    {
        result = add_student(&stu_list, cmd_argv[1], cmd_argv[2], courses,
                             num_courses);
        if (result == 1)
        {
            error("This student is already in the queue.", ptr);
        }
        else if (result == 2)
        {
            error("Invalid Course -- student not added.", ptr);
        }
    }
    else if (strcmp(cmd_argv[0], "print_full_queue") == 0 && cmd_argc == 1)
    {
        *ptr = print_full_queue(stu_list);
    }
    else if (strcmp(cmd_argv[0], "print_currently_serving") == 0 && cmd_argc == 1)
    {
        //print_currently_serving(ta_list);
        *ptr = print_currently_serving(ta_list);
    }
    else if (strcmp(cmd_argv[0], "give_up") == 0 && cmd_argc == 2)
    {
        if (give_up_waiting(&stu_list, cmd_argv[1]) == 1)
        {
            error("There was no student by that name waiting in the queue.", ptr);
        }
    }
    else if (strcmp(cmd_argv[0], "add_ta") == 0 && cmd_argc == 2)
    {
        add_ta(&ta_list, cmd_argv[1]);
    }
    else if (strcmp(cmd_argv[0], "remove_ta") == 0 && cmd_argc == 2)
    {
        if (remove_ta(&ta_list, cmd_argv[1]) == 1)
        {
            error("Invalid TA name.", ptr);
        }
    }
    else if (strcmp(cmd_argv[0], "next") == 0 && cmd_argc == 2)
    {
        if (next_overall(cmd_argv[1], &ta_list, &stu_list) == 1)
        {
            ;
            error("Invalid TA name.", ptr);
        }
    }
    else
    {
        error("Incorrect syntax.", ptr);
    }
    return 0;
}

int main(void)
{

    // Create the socket FD.
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        perror("server: socket");
        exit(1);
    }

    // Set information about the port (and IP) we want to be connected to.
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    // This should always be zero. On some systems, it won't error if you
    // forget, but on others, you'll get mysterious errors. So zero it.
    memset(&server.sin_zero, 0, 8);

    // Bind the selected port to the socket.
    if (bind(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("server: bind");
        close(sock_fd);
        exit(1);
    }

    // Announce willingness to accept connections on this socket.
    if (listen(sock_fd, MAX_BACKLOG) < 0)
    {
        perror("server: listen");
        close(sock_fd);
        exit(1);
    }

    // The client accept - message accept loop. First, we prepare to listen to multiple
    // file descriptors by initializing a set of file descriptors.

    ClientList *clients = client_list_new(sock_fd);

    while (1)
    {
        // select updates the fd_set it receives, so we always use a copy and retain the original.
        fd_set listen_fds = client_fdset_clone(clients);
        int nready = client_list_select(clients, &listen_fds);
        if (nready == -1)
        {
            perror("server: select");
            exit(1);
        }

        // Is it the original socket? Create a new connection ...
        if (FD_ISSET(sock_fd, &listen_fds))
        {
            accept_connection(clients);
            printf("Accepted connection\n");
        }

        for (Client *c = client_list_root(clients); c != NULL; c = client_next(c))
        {
            int fd = client_fd(c);
            if (fd != -1 && FD_ISSET(fd, &listen_fds))
            {
                int client_closed = 0;

                switch (client_state(c))
                {
                case S_PROMPT_USERNAME:
                    // prompt for username here
                    // mutates c.usernamee
                    break;
                case S_PROMPT_TYPE:
                    //prompt for type here.
                    // should also display motd once
                    // prompt is finish
                    break;
                case S_PROMPT_COMMANDS:
                    //prompt for commands here
                    break;
                case S_DISCONNECTED:
                default:
                    break;
                }

                if (client_closed)
                {
                    // mark client as disconnected
                    // does not actually disconnect the client
                    // until client_list_collect is run.
                    client_close(c);
                }
            }
        }

        // do garbage collection for dropped clients.
        client_list_collect(clients);
    }
}

int main_old(int argc, char *argv[])
{
    if (argc < 1 || argc > 2)
    {
        fprintf(stderr, "Usage: ./helpcentre [commands_filename]\n");
        exit(1);
    }
    int batch_mode = (argc == 3);
    char input[INPUT_BUFFER_SIZE];
    FILE *input_stream;

    if ((courses = malloc(sizeof(Course) * 3)) == NULL)
    {
        perror("malloc for course list\n");
        exit(1);
    }
    strcpy(courses[0].code, "CSC108");
    strcpy(courses[1].code, "CSC148");
    strcpy(courses[2].code, "CSC209");

    // for holding arguments to individual commands passed to sub-procedure
    char *cmd_argv[INPUT_ARG_MAX_NUM];
    int cmd_argc;

    if (batch_mode)
    {
        input_stream = fopen(argv[2], "r");
        if (input_stream == NULL)
        {
            perror("Error opening file");
            exit(1);
        }
    }
    else
    { // interactive mode
        input_stream = stdin;
    }

    printf("Welcome to the Help Centre Queuing System\nPlease type a command:\n>");

    while (fgets(input, INPUT_BUFFER_SIZE, input_stream) != NULL)
    {
        char *msg = NULL;
        // only echo the line in batch mode since in interactive mode the user
        // has just typed the line
        if (batch_mode)
        {
            printf("%s", input);
        }
        // tokenize arguments
        // Notice that this tokenizing is not sophisticated enough to
        // handle quoted arguments with spaces so names can not have spaces.
        char *next_token = strtok(input, DELIM);
        cmd_argc = 0;
        while (next_token != NULL)
        {
            if (cmd_argc >= INPUT_ARG_MAX_NUM)
            {
                error("Too many arguments.", &msg);
                cmd_argc = 0;
                break;
            }
            cmd_argv[cmd_argc] = next_token;
            cmd_argc++;
            next_token = strtok(NULL, DELIM);
        }

        if (cmd_argc > 0 && process_args(cmd_argc, cmd_argv, &msg) == -1)
        {
            break; // can only reach if quit command was entered
        }
        printf(">");
    }

    if (batch_mode)
    {
        fclose(input_stream);
    }
    return 0;
}
