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
#include "client.h"
#include "panic.h"

#ifndef PORT
#define PORT 30000
#endif
#define MAX_BACKLOG 3
#define DELIM " \n"

/* Print a formatted error message to stderr.
 */
int error(char *msg, char **buf)
{
    paasprintf(buf, "Error: %s\n", msg);
    return 0;
}

// Use global variables so we can have exactly one TA list and one student list
Ta *ta_list = NULL;
Student *stu_list = NULL;

Course *courses;
int num_courses = 3;

// Process the response from the client as its username,
// advancing the prompt to ask for the type of client.
int process_username(Client *c)
{
    char *msg = client_ready_message(c);
    client_set_username(c, msg);
    printf("Set client username to: %s\n", msg);
    
    // ask for the type next loop.
    client_set_state(c, S_PROMPT_TYPE);
    free(msg);
    return 0;
}

// Process the response from the client as its
// type.
// If the response is valid, then will transition the
// prompt state to S_PROMPT_MOTD, otherwise will
// ask again until a valid response
int process_type(Client *c)
{
    char *msg = client_ready_message(c);

    if (!strncmp("T", msg, 1))
    {
        client_set_type(c, CLIENT_TA);
        printf("Set client type to TA\n");

        client_set_state(c, S_PROMPT_MOTD);
    }
    else if (!strncmp("S", msg, 1))
    {
        client_set_type(c, CLIENT_STUDENT);
        printf("Set client type to Student\n");
        
        client_set_state(c, S_PROMPT_MOTD);
    }
    else
    {
        client_set_state(c, S_PROMPT_TYPE_INVALID);
    }
    free(msg);
    return 0;
}

// Process the response form the client as a 
// course code.
int process_course(Client *c)
{
    char *msg = client_ready_message(c);

    for (int i = 0; i < num_courses; i++)
    {
        if (!strcmp(courses[i].code, msg))
        {
            client_set_state(c, S_PROMPT_COMMANDS);
            client_write(c, "You have been entered into the queue. While you wait, you can "
                            "use the command stats to see which TAs are currently serving students.\r\n");
            
            // Add the student to the student list now that we know what course
            // they want to queue for.
            add_student(&stu_list, client_username(c), msg, courses, num_courses, c);

            free(msg);
            return 0;
        }
    }
    client_write(c, "This is not a valid course. Good-bye.\r\n");

    // set the state to invalid, then mark the client as disconnected 
    client_set_state(c, S_INVALID);
    client_close(c);
    free(msg);
    return 0;
}

int process_command(ClientList *l, Client *c)
{
    // tokenize arguments
    // Notice that this tokenizing is not sophisticated enough to
    // handle quoted arguments with spaces so names can not have spaces.
    char *input = client_ready_message(c);

    if (!strcmp("stats", input))
    {
        char *response = NULL;

        if (client_type(c) == CLIENT_STUDENT)
        {
            response = print_currently_serving(ta_list);
        }

        if (client_type(c) == CLIENT_TA)
        {
            response = print_full_queue(stu_list);
        }
        client_write(c, response);
        free(response);
    }
    else if (!strcmp("next", input) && client_type(c) == CLIENT_TA)
    {
        // Try and assign the TA a student
        next_overall(client_username(c), &ta_list, &stu_list);

        // Find the TA that this client is associated with
        Ta *ta = find_ta(ta_list, client_username(c));

        // If the TA has a student, disconnect the student.
        if (ta->current_student)
        {
            Client *st_client = ta->current_student->client;
            client_write(st_client, "Your turn to see the TA.\r\nWe are disconnecting you from the server now. Press Ctrl-C to close nc\r\n");
            
            // Although this will free the client belonging to the student, 
            // c is not touched and thus client_iter_next(c) will work, and
            // the iterator is still safe.

            // additionally, we want to keep the student allocated until 
            // it's freed by the hcq lifecycle functions, so indicate not
            // to free the student when freeing the associated client.
            client_list_remove(l, st_client, &ta_list, &stu_list, 1);
        }
    }
    else
    {
        client_write(c, "Incorrect syntax\r\n");
    }

    free(input);
    return 0;
}

int main(void)
{
    if ((courses = malloc(sizeof(Course) * 3)) == NULL)
    {
        perror("malloc for course list\n");
        exit(1);
    }

    strcpy(courses[0].code, "CSC108");
    strcpy(courses[1].code, "CSC148");
    strcpy(courses[2].code, "CSC209");

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

    // Ensure port is freed when process terminates
    int on = 1;
    int status = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR,
                            (const char *)&on, sizeof(on));
    if (status == -1)
    {
        perror("setsockopt -- REUSEADDR");
        exit(1);
    }

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

   
    // Initialize a list of clients to manage.
    // All socket operations should now take place within this ClientList context.
    ClientList *clients = client_list_new(sock_fd);

    while (1)
    {
        // Garbage collect disconnected clients before trying to send prompts.
        client_list_collect(clients, &ta_list, &stu_list);

        // Send prompts for every client preparing to be ready for reading from.
        // Notice that client_prep_read must be called after the prompt, 
        // without this call the client would not be able to be read from.

        // See the documentation for client_prep_read in client.h
        for (Client *c = client_iter_begin(clients); c != NULL; c = client_iter_next(c))
        {
            if (client_recv_state(c) == RS_RECV_PREP)
            {
                switch (client_state(c))
                {
                case S_INVALID:
                    continue;

                case S_PROMPT_USERNAME:
                    client_write(c, "Welcome to the Help Centre, what is your name?\r\n");
                    client_prep_read(c);
                    break;
                case S_PROMPT_TYPE:
                    client_write(c, "Are you a TA or a Student (enter T or S)?\r\n");
                    client_prep_read(c);
                    break;
                case S_PROMPT_TYPE_INVALID:
                    client_write(c, "Invalid role (enter T or S)?\r\n");
                    client_prep_read(c);
                    break;
                case S_PROMPT_MOTD:
                    // Nested switch is sketchy, but the alternative are
                    // gotos.
                    switch (client_type(c))
                    {
                    case CLIENT_TA:
                        client_write(c, "Valid commands for TA:\r\n\tstats\r\n\tnext\r\n\t(or use Ctrl-C to leave)\r\n");
                        
                        // If the client is a TA, we have all the information we need to add the TA to the 
                        // TA list.
                        add_ta(&ta_list, client_username(c), c);

                        // prompt commands on next loop iteration.
                        client_set_state(c, S_PROMPT_COMMANDS);
                        break;
                    case CLIENT_STUDENT:
                        client_write(c, "Valid courses: ");
                        for (int i = 0; i < num_courses; i++)
                        {
                            client_write(c, courses[i].code);
                            if (i < num_courses - 1)
                            {
                                client_write(c, ", ");
                            }
                        }
                        client_write(c, "\r\nWhich course are you asking about?\r\n");
                        client_set_state(c, S_PROMPT_COURSES);
                        // after this prompt, if the choice is valid, the student will be
                        // created and added to the student list.
                        break;
                    case CLIENT_UNSET:

                        // should never reach here
                        break;
                    }
                    client_prep_read(c);
                    break;
                case S_PROMPT_COMMANDS:
                    client_prep_read(c);
                    break;
                default:
                    break;
                }
            }
        }

        // Prompts could have dropped some clients.
        // do garbage collection for dropped clients.
        // any I/O operation on a dropped socket will set the recv state to RS_DISCONNECTED.
        // this will mark the client as dead.
        client_list_collect(clients, &ta_list, &stu_list);

        ClientSocketSet listen_fds = client_list_fdset_clone(clients);
        int nready = client_list_select(clients, &listen_fds);
        if (nready == -1)
        {
            perror("server: select");
            exit(1);
        }

        // Is it the original socket? Create a new connection ...
        if (FD_ISSET(sock_fd, &listen_fds))
        {
            client_list_accept_connection(clients);
            printf("Accepted connection\n");
        }

        for (Client *c = client_iter_begin(clients); c != NULL; c = client_iter_next(c))
        {
            int fd = client_fd(c);
            if (fd != -1 && FD_ISSET(fd, &listen_fds))
            {
                // client_recv_state may change in between calls.
                // Only do a read if the state is RS_RECV_NOT_RDY.
                if (client_recv_state(c) == RS_RECV_NOT_RDY)
                {
                    client_read(c);
                }

                // last read may have changed the recv state.
                // if so, process the mesage.
                if (client_recv_state(c) == RS_RECV_RDY)
                {
                    switch (client_state(c))
                    {
                    case S_PROMPT_USERNAME:
                        process_username(c);
                        break;
                    case S_PROMPT_TYPE:
                    case S_PROMPT_TYPE_INVALID:
                        process_type(c);
                        break;
                    case S_PROMPT_COURSES:
                        process_course(c);
                        break;
                    case S_PROMPT_COMMANDS:
                        process_command(clients, c);
                        break;
                    case S_INVALID:
                        continue;
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        // Processing could have dropped some clients.
        // do garbage collection for dropped clients.
        // any I/O operation on a dropped socket will set the recv state to RS_DISCONNECTED.
        // this will mark the client as dead.
        client_list_collect(clients, &ta_list, &stu_list);
    }
}
