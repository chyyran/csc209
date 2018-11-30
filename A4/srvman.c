#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <assert.h>
#include "srvman.h"

#define INPUT_BUFFER_SIZE 256
#define INPUT_ARG_MAX_NUM 3
#define DELIM " \n"

typedef struct client_s
{
    int sock_fd;
    ClientType type;
    char name[INPUT_BUFFER_SIZE];
    char buffer[INPUT_BUFFER_SIZE];
    Client *next;
    Client *prev;
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
    return ptr;
}

ClientInitState client_state(Client *c)
{
    return c->state;
}

Client *client_next_state(Client *c)
{
    switch (c->state)
    {
    case S_PROMPT_USERNAME:
        c->state = S_PROMPT_TYPE;
        break;
    case S_PROMPT_TYPE:
        c->state = S_PROMPT_COMMANDS;
    case S_PROMPT_COMMANDS:
        c->state = S_DISCONNECTED;
    default:
        c->state = c->state;
        break;
    }
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

ClientList *client_list_append(ClientList *l, Client *c)
{
    c->prev = l->end;
    c->next = NULL;
    l->end->next = c;
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
    c->state = S_DISCONNECTED;
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
Client *client_list_remove(ClientList *l, Client *c)
{
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
        c->prev->next = NULL;
    }
    if (c->prev == NULL)
    {
        // c is the first client
        // attach root to the next client.
        l->root = c->next;
    }
    FD_CLR(c->sock_fd, &l->fds);
    // free the client
    free(c);
    return prev;
}

ClientList *client_list_collect(ClientList *l)
{
    for (Client *c = client_list_root(l); c != NULL; c = client_next(c))
    {
        if (c->state == S_DISCONNECTED)
        {
            c = client_list_remove(l, c);
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
