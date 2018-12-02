#ifndef SRVMAN_H
#define SRVMAN_H
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/** Forward declarations for hcq.h types **/
typedef struct student Student;
typedef struct course Course;
typedef struct ta Ta;
int give_up_waiting(Student **stu_list_ptr, char *student_name);
int remove_ta(Ta **ta_list_ptr, char *ta_name);

/**
 * A Client encapsulates a socket and its associated buffers used for
 * readng an writing safely to the socket. 
 * 
 * It also holds metadata about the type and username of Client, 
 * and keeps track of prompt and socket communication state.
 * 
 * The general idea when interfacing with a Client is like so
 * 
 * 1. If the client is preparing to receive a message (RS_RECV_PREP), 
 *    write to the client according to its prompt state. Then update 
 *    its socket communication state to RS_RECV_NOT_RDY, indicating
 *    a partial read.
 * 
 * 2. Read from the client if its socket communication state is
 *    RS_RECV_NOT_RDY, storing the data into a growable buffer. 
 *    If the read is not complete, do not update the communication
 *    state and continue with the next client. Otherwise, change 
 *    the state to RS_RECV_RDY, indicating that this client has read
 *    a full message.
 * 
 * 3. If the client has a message ready (RS_RECV_RDY), get the message
 *    from the Client, and write a response. If necessary, change the
 *    prompt state to prompt the Client for a different message next
 *    loop. Afterwards, refresh the Client's buffers and set the 
 *    socket communication state to RS_RECV_PREP.
 * 
 * If at any point the Client is destroyed or a read or write fails, the
 * socket communication state is immediately set to RS_RECV_DISCONNECTED,
 * and all further communication to the Client is disabled.
 * 
 * It is prudent to note that a Client does not own a Ta or a Student, since
 * Clients are created once a connection is accepted, before it is known 
 * whether a Client is a Ta or a Student. Instead, a Ta or Student should be
 * created at the earliest possible oppurtunity when this becomes known, and
 * that Ta or Student should hold a non-owning reference to the associated
 * Client. 
 * 
 * Each Client also implements a doubly linked list of other clients, but 
 * operations on this linked list of clients should be done within the 
 * context of a ClientList or the client_iter_* APIs.
 */
typedef struct client_s Client;

/**
 * A ClientList encapsulates a collection of Clients and their socket file 
 * descriptor set. 
 * 
 * The Clients in this ClientList can not be manipulated on directly, and
 * should only be iterated on through the client_iter_* APIs. 
 * 
 * This list also serves as the context within which clients are selected
 * on and their sockets kept track of, as well as being self-cleaning, 
 * ensuring that dead Clients are removed on the list.
 */
typedef struct client_list_s ClientList;

/**
 * Type alias for fd_set returned by ClientList socket select
 * functions.
 */ 
typedef fd_set ClientSocketSet;

/**
 * The possible types of Client that the connected client could
 * become.
 */
typedef enum client_type_e
{
    /**
     * CLIENT_UNSET indicates that either the client is yet to determine
     * whether they are a TA or a STUDENT, or that the client is soon
     * to be disconnected and garbage colected from the ClientList context.
    */
    CLIENT_UNSET = 0,
    /** 
     * CLIENT_TA indicates that the client is a TA, and should have access 
     * to commands used by TAs.
     */
    CLIENT_TA = 1,

    /**
     * CLIENT_STUDENT indicates that the client is a Student, and should have
     * access to commands used by Students.
     */
    CLIENT_STUDENT = 2,
} ClientType;

/**
 * The possible things to prompt the client on the next read cycle.
 */
typedef enum client_prompt_state_e
{
    /**
     * Prompt the client for their username.
     * 
     * The proper transition for this State is
     * to S_PROMPT_TYPE.
     */
    S_PROMPT_USERNAME = 0,

    /**
     * Prompt the client for their type (TA or Student)
     * 
     * The proper transition for this State is to
     * S_PROMPT_TYPE_INVALID if the response is invalid,
     * or to S_PROMPT_MOTD if their response is valid.
     * 
     * If the client is a Student, the proper transition
     * is to S_PROMPT_COURSES rather than S_PROMPT_MOTD.
     */
    S_PROMPT_TYPE = 1,

    /**
     * Prompt the client for their type (TA or Student),
     * indicating that their previous choice was invalid.
     * 
     * The proper transition for this State is to
     * S_PROMPT_TYPE_INVALID if the response is invalid,
     * or to S_PROMPT_MOTD if their response is valid.
     */
    S_PROMPT_TYPE_INVALID = 2,

    /**
     * Shows the client the message of the day, (their valid
     * commands, depending on their type), and should then 
     * immediately transition to the S_PROMPT_COMMANDS state
     * without prompting the client for any input.
     */
    S_PROMPT_MOTD = 3,

    /**
     * Prompt the client for a command, without displaying
     * any message. 
     * 
     * The proper transition for this State is to S_PROMPT_COMMANDS
     * after each command has been received and their response
     * sent to the client.
     */
    S_PROMPT_COMMANDS = 4,

    /**
     * Prompt the client for a choice of class, if the client 
     * is a Student. TA clients should never reach this state.
     * 
     * The proper transition for this State is to S_PROMPT_MOTD
     * if the class exists, otherwise, the transition is to 
     * S_INVALID. In this case, the socket communication state
     * should also be immediately changed to RS_DISCONNECTED, and
     * the Client should be removed from the ClientList context
     * at the next possible oppurtinity.
     */
    S_PROMPT_COURSES = 5,

    /**
     * When the Client reaches an invalid state, and is about
     * to be disconnected.
     */
    S_INVALID = 100000,
} ClientPromptState;

/**
 * The possible states of the Client's socket communication
 * buffers.
 */
typedef enum client_recv_state_e
{
    /**
     * The client is ready to prepare its buffers, and
     * initiate a read. There is no guarantee as to the
     * contents of the buffers however, and attempting
     * to read any complete messages from this client
     * will result in an error.
     * 
     * This is the state that a Client first starts
     * out in, and consuming a ready message from
     * this client with client_ready_message will also
     * return the communication state to RS_RECV_PREP.
     * 
     * client_prep_read must be called to prepare its 
     * socket communication buffers at this state to
     * move into the RS_RECV_NOT_RDY state.
     */
    RS_RECV_PREP = 0,

    /**
     * The Client is not yet ready to have a message
     * consumed. It is in a state of partial read, and
     * the server should read from its socket once more
     * when ready.
     * 
     * Once the message has fully arrived, the state will
     * change to RS_RECV_READY.
     */
    RS_RECV_NOT_RDY = 1,

    /**
     * A full message has been received from the Client, and
     * is ready to be consumed by client_ready_message.
     * 
     * No further socket reads can occur on this Client until the
     * message is consumed.
     */
    RS_RECV_RDY = 2,

    /**
     * The Client has disconnected, or will be disconnected at
     * the earliest possible oppurtunities. No further operations
     * may be done on this Client except to close it.
     */
    RS_DISCONNECTED = 3,
} ClientRecvState;

/**
 * Creates a new Client with the given socket file descriptor.
 * 
 * The returned Client is not attached to any ClientList, but
 * must be eventually before it can be freed.
 */ 
Client *client_new(int sock_fd);

/**
 * Gets the username of the client. Will return
 * an empty string if not set.
 */
const char *client_username(Client *c);

/**
 * Sets the username of the client. The username is
 * automatically truncated to 30 characters.
 */
Client *client_set_username(Client *c, const char *username);

/**
 * Gets the type of the client.
 */
ClientType client_type(Client *c);

/**
 * Sets the type of the client.
 */ 
Client *client_set_type(Client *c, ClientType type);

/**
 * Gets the prompt state of the client.
 */ 
ClientPromptState client_state(Client *c);

/**
 * Sets the prompt state of the client.
 */ 
Client *client_set_state(Client *c, ClientPromptState s);

/**
 * Gets the Client socket communication state.
 * This state is automaticaly updated when using
 * the Client socket lifecycle functions
 *   - client_write
 *   - client_prep_read
 *   - client_read
 *   - client_ready_message
 *   - client_close
 */ 
ClientRecvState client_recv_state(Client *c);

/**
 * Writes a message to the Client's socket, returning
 * the number of bytes written.
 * 
 * The message is automatically truncated to 2048 
 * characters before being sent to the socket for
 * security reasons.
 * 
 * This function does not retry the write if the
 * message was not fully sent.
 * 
 * If the message fails to send, the given client is
 * marked as disconnected, and further communication
 * with the Client is disabled.
 * 
 * If the Client is in the RS_DISCONNECTED state,
 * this function will return -1 and do nothing.
 */
int client_write(Client *c, const char *message);

/**
 * Prepares the Client's socket read buffers for
 * reading, updating the Client's socket communication
 * state to RS_RECV_NOT_RDY.
 * 
 * If the Client is not in the RS_RECV_PREP state, 
 * this function will return -1 and do nothing.
 */ 
int client_prep_read(Client *c);

/**
 * Reads up to 30 bytes from the client socket. If the read is
 * incomplete, or a network newline is not found, then
 * the resulting data is saved in an internal buffer for the
 * next read.
 * 
 * If the read has completed, then any overflow data will be saved
 * to a separate buffer for the next read. The Client will have its 
 * communication state changed to RS_RECV_RDY, and no further
 * reads will be possible from this Client until client_ready_message
 * is called to retrieve the fully sent message.
 * 
 * If the read fails, the client will be marked as disconnected,
 * and further communication with the client is disabled.
 * 
 * If the client is not in the RS_RECV_NOT_RDY, state, this
 * function will return -1 and do nothing.
 */ 
int client_read(Client *c);

/**
 * Retrieves (and consumes from the internal buffer), the
 * message that was read from the Client socket, returning
 * a pointer to the message.
 * 
 * Once the message is processed, it should be freed. Calling
 * this function automatically invalidates the Client's internal
 * read buffers until client_prep_read is called. It will also
 * change the socket communication state to RS_RECV_PREP, and
 * as such, can only be called once per fully-read message.
 */
char *client_ready_message(Client *c);

/**
 * Marks the client as disconnected, but does not free
 * the client. 
 * 
 * A client can not be freed directly except within the
 * context of a ClientList using the client_list_collect
 * function to "garbage-collect" disconnected clients,
 * or client_list_remove to remove a given client from
 * the ClientList context, freeing resources in the process.
 * 
 * This function merely marks the Client to be freed at
 * the next possible oppurtunity, but does not release 
 * any owned resources.
 */ 
void client_close(Client *c);

/**
 * Returns the client at the root of the ClientList.
 *  
 * Used to iterate through clients in a ClientList context.
 * Use with client_iter_next to advance the iterator.
 * 
 * Do not remove any Clients from the ClientList context
 * while an iterating through the ClientList. Doing so
 * will not invalidate the iterator, but may cause
 * undefined behaviour if an operation is attempted on
 * a removed client.
 * 
 * Instead, mark the client as disconnected using
 * client_close, then call client_list_collect after
 * the iterator ends, to ensure that removals are safe.
 */ 
Client *client_iter_begin(ClientList *c);

/**
 * Returns the next Client in the ClientList in the
 * iteration. 
 * 
 * Used with client_iter_begin to obtain an iterator,
 * or a reference to the first Client.
 * 
 * Do not remove any Clients from the ClientList context
 * while an iterating through the ClientList. Doing so
 * will not invalidate the iterator, but may cause
 * undefined behaviour if an operation is attempted on
 * a removed client.
 * 
 * Instead, mark the client as disconnected using
 * client_close, then call client_list_collect after
 * the iterator ends, to ensure that removals are safe.
 */
Client *client_iter_next(Client *c);

/**
 * Creates a new ClientList context with the listener
 * socket file descriptor. 
 */
ClientList *client_list_new(int sock_fd);

/**
 * Appends a Client to the end of this ClientList context,
 * attaching it to the context. 
 */
ClientList *client_list_append(ClientList *l, Client *c);

/**
 * Remove a Client from the ClientList context, preserving
 * order within remaining Clients, and returning the Client
 * previous to the removed Client (NULL if the removed Client 
 * is the first in the ClientList)
 * 
 * Calling this function will free any and all resources 
 * associated with the Client, including owned sockets,
 * buffers, and the Client itself. The Client will be
 * disconnected from the server, regardless of its 
 * socket communication state.
 * 
 * If the Client's type is set, the corresponding Ta or Student
 * with the same name as the Client's username will be removed
 * from the list and freed. 
 * 
 * The Client's socket file descriptor will also be cleared from
 * the set of file descriptors to be selected on from 
 * client_list_select.
 */ 
Client *client_list_remove(ClientList *l, Client *c, Ta **ta_list, Student **student_list);

/**
 * Iterates through all Clients in the ClientList context,
 * and removes those that have a socket communication state
 * of RS_DISCONNECTED. 
 * 
 * client_list_remove will be called on such sockets, freeing
 * all owned resources. See client_list_remove for a full
 * list of side effects freeing a client involves.
 */ 
ClientList *client_list_collect(ClientList *l, Ta **ta_list, Student **student_list);

/**
 * Returns the socket file descriptor used to 
 * communicate with this client.
 */ 
int client_fd(Client *c);

/**
 * Waits until one or more Clients (their underlying sockets)
 * are ready to be read from, returning the number of file
 * descriptors ready to be read.
 * 
 * The provied ClientSocketSet is simply a type alias for
 * an fd_set, and standard FD_* macros can and should be used
 * to query the set.
 */
int client_list_select(ClientList *l, ClientSocketSet *out_fds);

/**
 * Returns a copy of the ClientList's internal file descriptor
 * set for use with client_list_select.
 */ 
ClientSocketSet client_list_fdset_clone(ClientList *l);

/**
 * Accepts a pending connection on the listening socket
 * managed by the ClientList, creating a new Client
 * to encapsulate socket communication with the
 * new connection.
 */
void client_list_accept_connection(ClientList *l);

#endif // SRVMAN_H
