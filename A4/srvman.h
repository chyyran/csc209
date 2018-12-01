#ifndef SRVMAN_H
#define SRVMAN_H
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct client_s Client;
typedef struct client_list_s ClientList;

typedef enum client_type_e
{
    CLIENT_UNSET = 0,
    CLIENT_TA = 1,
    CLIENT_STUDENT = 2,
} ClientType;

typedef enum client_init_state_e
{
    S_PROMPT_USERNAME = 0,
    S_PROMPT_TYPE = 1,
    S_PROMPT_TYPE_INVALID = 2,
    S_PROMPT_MOTD = 3,
    S_PROMPT_COMMANDS = 4,
    S_PROMPT_COURSES = 5,
    S_INVALID = 100000,
} ClientInitState;

typedef enum client_recv_state_e
{
    RS_RECV_PREP = 0,
    RS_RECV_NOT_RDY = 1,
    RS_RECV_RDY = 2,
    RS_DISCONNECTED = 3,
} ClientRecvState;

Client *client_new(int sock_fd);
Client *client_set_state(Client *c, ClientInitState s);
Client *client_set_username(Client *c, const char *username);
void client_close(Client *c);
Client *client_next(Client *c);
int client_fd(Client *c);
int client_prep_read(Client *c);

ClientType client_type(Client* c);
Client *client_set_type(Client *c, ClientType type);


ClientRecvState client_recv_state(Client *c);
ClientInitState client_state(Client *c);
int client_write(Client *c, const char *message);
int client_read(Client *c);
char *client_ready_message(Client *c);

ClientList *client_list_new(int sock_fd);
ClientList *client_list_append(ClientList *l, Client *c);
ClientList *client_list_collect(ClientList *l);

Client *client_list_remove(ClientList *l, Client *c);
Client *client_list_root(ClientList *c);

int client_list_select(ClientList *l, fd_set *out_fds);

fd_set client_fdset_clone(ClientList *l);
void accept_connection(ClientList *l);

#endif // SRVMAN_H
