#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <assert.h>
#include <stdbool.h>

#include "client.h"
#include "dynstr.h"

#define INPUT_BUFFER_SIZE 30
#define OUTPUT_BUFFER_SIZE 2048
#define INPUT_ARG_MAX_NUM 3
#define DELIM " \n"
int find_network_newline(const char *buf, int n);
typedef struct client_s
{
    int sock_fd;
    ClientType type;
    char name[INPUT_BUFFER_SIZE + 1];
    char wbuffer[OUTPUT_BUFFER_SIZE];
    char roverflow[INPUT_BUFFER_SIZE];
    DynamicString *rbuffer;

    Client *next;
    Client *prev;
    ClientRecvState recv;
    ClientInitState state;
} client_s;

typedef struct client_list_s
{
    Client *root;
    Client *end;
    fd_set fds;
    int max_fd;
    int sock_fd;
} client_list_s;

ClientList *client_list_new(int sock_fd)
{
    ClientList *ptr = calloc(1, sizeof(ClientList));
    ptr->root = NULL;
    ptr->sock_fd = sock_fd;
    ptr->max_fd = sock_fd;
    FD_ZERO(&ptr->fds);
    FD_SET(sock_fd, &ptr->fds);
    return ptr;
}

Client *client_new(int sock_fd)
{
    Client *ptr = calloc(1, sizeof(Client));
    ptr->sock_fd = sock_fd;
    ptr->type = CLIENT_UNSET;
    ptr->state = S_PROMPT_USERNAME;
    ptr->next = NULL;
    ptr->prev = NULL;
    ptr->rbuffer = NULL;
    ptr->recv = RS_RECV_PREP;
    return ptr;
}

ClientType client_type(Client *c)
{
    return c->type;
}

ClientInitState client_state(Client *c)
{
    return c->state;
}

Client *client_set_state(Client *c, ClientInitState state)
{
    c->state = state;
    return c;
}

Client *client_next(Client *c)
{
    if (c == NULL)
        return NULL;
    return c->next;
}

Client *client_list_root(ClientList *l)
{
    return l->root;
}

const char *client_username(Client *c)
{
    return c->name;
}

Client *client_set_username(Client *c, const char *username)
{
    memset(c->name, 0, INPUT_BUFFER_SIZE + 1);
    strncpy(c->name, username, INPUT_BUFFER_SIZE);
    c->name[INPUT_BUFFER_SIZE] = '\0';
    return c;
}

Client *client_set_type(Client *c, ClientType type)
{
    c->type = type;
    return c;
}
ClientList *client_list_append(ClientList *l, Client *c)
{
    if (l->root == NULL)
    {
        l->root = c;
    }
    c->prev = l->end;
    c->next = NULL;
    if (l->end != NULL)
    {
        l->end->next = c;
    }
    l->end = c;
    FD_SET(c->sock_fd, &l->fds);

    if (c->sock_fd > l->max_fd)
    {
        l->max_fd = c->sock_fd;
    }
    return l;
}

int client_fd(Client *c)
{
    return c->sock_fd;
}

void client_close(Client *c)
{
    c->recv = RS_DISCONNECTED;
}

fd_set client_fdset_clone(ClientList *l)
{
    return l->fds;
}

int client_list_select(ClientList *l, fd_set *out_fds)
{
    return select(l->max_fd + 1, out_fds, NULL, NULL, NULL);
}

// returns the previous
Client *client_list_remove(ClientList *l, Client *c, Ta **ta_list, Student **student_list)
{
    printf("Disconnecting client %d\n", c->sock_fd);
    assert(c != NULL);
    assert(l != NULL);
    Client *prev = c->prev;
    if (c->next != NULL && c->prev != NULL)
    {
        // c is between two clients.
        c->prev->next = c->next;
    }

    if (c->next == NULL)
    {
        // c is the last client:
        // update the list tail, and
        // delete the next.
        l->end = c->prev;
        if (c->prev != NULL)
        {
            c->prev->next = NULL;
        }
    }
    if (c->prev == NULL)
    {
        // c is the first client
        // attach root to the next client.
        l->root = c->next;
    }

    if (c->type == CLIENT_STUDENT)
    {
        give_up_waiting(student_list, c->name);
    }
    if (c->type == CLIENT_TA)
    {
        remove_ta(ta_list, c->name);
    }

    if (c->sock_fd <= FD_SETSIZE && c->sock_fd >= 0) {
        FD_CLR(c->sock_fd, &l->fds);
    }
    // We do not care about the return value of this close call.
    // If it fails, the client is destroyed regardless.
    // If this close fails, or example if sock_fd is -1, then
    // there's nothing we can do anyways.

    // There is no way to write or read to this client anymore once
    // this function returns.
    close(c->sock_fd);
    printf("Client disconnected %d\n", c->sock_fd);
    // free the client
    free(c);
    return prev;
}

ClientList *client_list_collect(ClientList *l, Ta **ta_list, Student **student_list)
{
    for (Client *c = client_list_root(l); c != NULL; c = client_next(c))
    {
        if (c->recv == RS_DISCONNECTED)
        {
            c = client_list_remove(l, c, ta_list, student_list);
        }
    }

    return l;
}

void accept_connection(ClientList *l)
{
    int client_fd = accept(l->sock_fd, NULL, NULL);
    if (client_fd < 0)
    {
        perror("server: accept");
        close(l->sock_fd);
        exit(1);
    }

    Client *c = client_new(client_fd);
    client_list_append(l, c);
}

char *client_ready_message(Client *c)
{
    char *ptr = ds_into_raw_truncate(c->rbuffer, INPUT_BUFFER_SIZE);
    c->recv = RS_RECV_PREP;
    c->rbuffer = NULL;
    return ptr;
}

ClientRecvState client_recv_state(Client *c)
{
    return c->recv;
}

int client_write(Client *c, const char *message)
{
    if (c->sock_fd == -1 || c->recv == RS_DISCONNECTED)
        return -1;

    memset(c->wbuffer, 0, OUTPUT_BUFFER_SIZE);
    snprintf(c->wbuffer, OUTPUT_BUFFER_SIZE - 1, "%s", message);
    int nbytes = write(c->sock_fd, c->wbuffer, strlen(c->wbuffer));

    if (nbytes == -1) {
        printf("Tried to write to broken pipe, marking client as dead\n");
        c->recv = RS_DISCONNECTED;
    }
    return nbytes;
}

int client_prep_read(Client *c)
{
    if (c->recv != RS_RECV_PREP || c->rbuffer != NULL)
    {
        return -1;
    }
    c->rbuffer = ds_new(0);

    // push overflow into the read buffer.
    if (c->roverflow[0] != '\0') {
        printf("Refreshed overflow |%s|\n", c->roverflow);
        ds_append(c->rbuffer, c->roverflow);
        memset(c->roverflow, 0, INPUT_BUFFER_SIZE);
    }

    c->recv = RS_RECV_NOT_RDY;
    return 0;
}

int client_read(Client *c)
{
    if (c->sock_fd == -1 || c->recv == RS_DISCONNECTED)
        return -1;
    if (c->recv == RS_RECV_RDY)
    {
        // receive to read if buffer is ready
        return -1;
    }
    if (c->recv == RS_RECV_PREP)
    {
        return -1;
    }

    char buf[INPUT_BUFFER_SIZE + 3] = {'\0'};
    int nbytes = read(c->sock_fd, buf, INPUT_BUFFER_SIZE);
    if (nbytes <= 0)
    {
        printf("Tried to read from broken pipe, marking client as dead\n");
        c->recv = RS_DISCONNECTED;
        return -1;
    }
    int crlf = -1;
    if ((crlf = find_network_newline(buf, INPUT_BUFFER_SIZE)) == -1)
    {
        // no newline found, append the entire thing
        ds_append(c->rbuffer, buf);
    }
    else
    {
        buf[crlf - 2] = '\0';

        // put overflow reads into the overflow buffer.
        memset(c->roverflow, 0, INPUT_BUFFER_SIZE);
        memcpy(c->roverflow, &buf[crlf], INPUT_BUFFER_SIZE - crlf);

        //todo: put remainin stuff in buffer.
        ds_append(c->rbuffer, buf);
        // flag as ready
        c->recv = RS_RECV_RDY;
    }
    return nbytes;
}

/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return one plus the index of the '\n' of the first network newline,
 * or -1 if no network newline is found.
 * Definitely do not use strchr or other string functions to search here. (Why not?)
 */
int find_network_newline(const char *buf, int n)
{
    for (int i = 0; i < n - 1; i++)
    {
        // if buf[i] is not CR, then we don't care about buf[i+1]
        // if we're at buf[n -1], and buf[n-1] is not CR, then we don't care if buf[n] is CR.

        if (buf[i] == '\r' && buf[i + 1] == '\n')
            return i + 2; //LF is at i+1
    }
    return -1;
}