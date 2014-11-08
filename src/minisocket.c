/*
 *	Implementation of minisockets.
 */
#include "defs.h"
#include "minisocket.h"
#include "miniheader.h"
#include "minimsg.h"
#include "network.h"
#include "queue.h"
#include "synch.h"
#include "alarm.h"

#define BASE_DELAY 100

enum { S_LISTENING = 1, S_RESPONDING, S_CONNECTED, S_CLOSING }; // Server state
enum { C_ESTABLISHING = 1, C_CONNECTED, C_CLOSING }; // Client state

struct minisocket {
    char socket_type;
    int port_number;
    // Remote connection
    network_address_t remote_address;
    int remote_port;

    int seq; // SEQ number
    int ack; // ACK number

    int num_sent; // Number of times we have sent the same packet
    int timeout; // Current delay
    alarm_id retry_alarm; // Alarm for resending packets

    union {
        struct {
            char server_state;
        } server;
        struct {
            char client_state;
        } client;
    } u;
};

semaphore_t mutex_server; // Lock for server sockets
semaphore_t mutex_client; // Lock for client sockets

minisocket_t server_ports[NUMPORTS]; // Array of all the server sockets
minisocket_t client_ports[NUMPORTS]; // Array of all the client sockets
int next_client_id; // Next client port id to use

/*
 * Handler for receiving a message on a socket
 * Assumes interrupts are disabled throughout
 */
void
minisocket_handle(network_interrupt_arg_t *arg) {

}

/* Increments the client id
 */
void
increment_client_id() {
    if (++next_client_id >= NUMPORTS) {
        next_client_id = 0;
    }
}

/* Initializes the minisocket layer. */
void
minisocket_initialize() {
    int i;
    // Initialize global id counter
    next_client_id = 0;
    // Initialize both arrays to NULL
    for (i = 0; i < NUMPORTS; i++) {
        server_ports[i] = NULL;
        client_ports[i] = NULL;
    }
    // Initialize mutices
    mutex_server = semaphore_create();
    mutex_client = semaphore_create();
    // Null check
    if ( !mutex_server || !mutex_client ) {
        semaphore_destroy(mutex_server);
        semaphore_destroy(mutex_client);
        return;
    }

    semaphore_initialize(mutex_server, 1);
    semaphore_initialize(mutex_client, 1);
}

/* Constructs a new server socket
 * Returns -1 upon memory failure
 */
int
new_server(int port) {
    minisocket_t socket = (minisocket_t) malloc (sizeof(struct minisocket));

    if ( !socket ) return -1;

    // Generic port data
    socket->socket_type = SERVER;
    socket->port_number = port;

    socket->seq = 1;
    socket->ack = 0;
    socket->num_sent = 0;
    socket->timeout= BASE_DELAY;
    // TODO: MORE STUFF
    socket->u.server.server_state = S_LISTENING;

    // Successfully created a server
    server_ports[port] = socket;
    return 0;
}

/* Constructs a new client socket
 * Returns -1 upon memory failure
 */
int
new_client(int client_id, network_address_t addr, int port) {
    minisocket_t socket = (minisocket_t) malloc (sizeof(struct minisocket));

    if ( !socket ) return -1;

    // Generic port data
    socket->socket_type = CLIENT;
    socket->port_number = client_id + NUMPORTS;

    socket->seq = 1;
    socket->ack = 0;
    socket->num_sent = 0;
    socket->timeout = BASE_DELAY;

    socket->remote_address[0] = addr[0];
    socket->remote_address[1] = addr[1];
    socket->remote_port = port;
    // TODO: MORE STUFF
    socket->u.client.client_state = C_ESTABLISHING;

    // Successfully created a client
    client_ports[client_id] = socket;
    return 0;
}

/* Listens and blocks until connection successfully made with a client
 * Returns a minisocket upon success
 */
minisocket_t
server_handshake(minisocket_t socket, minisocket_error *error) {
    while (1) {
        switch (socket->u.server.server_state) {
            case S_LISTENING: // Listening for Syn
                break;
            case S_RESPONDING: // Sending Synacks
                break;
            case S_CONNECTED: // Received Ack
                *error = SOCKET_NOERROR;
                return socket;
            case S_CLOSING: // Socket closed
                break;
        }
    }
}

/* Tries to connect to a server
 * Returns a minisocket upon success
 */
minisocket_t
client_handshake(minisocket_t socket, minisocket_error *error) {
    while (1) {
        switch (socket->u.client.client_state) {
            case C_ESTABLISHING: // Sending Syn
                break;
            case C_CONNECTED: // Received Synack
                *error = SOCKET_NOERROR;
                return socket;
            case C_CLOSING: // Socket closed
                break;
        }
    }
}

/*
 * Listen for a connection from somebody else. When communication link is
 * created return a minisocket_t through which the communication can be made
 * from now on.
 *
 * The argument "port" is the port number on the local machine to which the
 * client will connect.
 *
 * Return value: the minisocket_t created, otherwise NULL with the errorcode
 * stored in the "error" variable.
 */
minisocket_t
minisocket_server_create(int port, minisocket_error *error) {
    // Out of range check
    if ( port < 0 || port >= NUMPORTS ) {
        *error = SOCKET_INVALIDPARAMS;
        return NULL;
    }

    semaphore_P(mutex_server); // Acquire lock
    if (server_ports[port] == NULL) { // Create new socket if didn't exist
        if (new_server(port) == -1) { // Failed in creating new socket
            semaphore_V(mutex_server);
            *error = SOCKET_OUTOFMEMORY;
            return NULL;
        }
    } else { // Port already exists
        semaphore_V(mutex_server); // Release lock
        *error = SOCKET_PORTINUSE;
        return NULL;
    }
    // Suceeded in creating new socket
    semaphore_V(mutex_server); // Release lock

    return server_handshake(server_ports[port], error);
}


/*
 * Initiate the communication with a remote site. When communication is
 * established create a minisocket through which the communication can be made
 * from now on.
 *
 * The first argument is the network address of the remote machine.
 *
 * The argument "port" is the port number on the remote machine to which the
 * connection is made. The port number of the local machine is one of the free
 * port numbers.
 *
 * Return value: the minisocket_t created, otherwise NULL with the errorcode
 * stored in the "error" variable.
 */
minisocket_t
minisocket_client_create(network_address_t addr, int port, minisocket_error *error) {
    int i;
    int cur_id;

    // Out of range check
    if ( port < 0 || port >= NUMPORTS ) {
        *error = SOCKET_INVALIDPARAMS;
        return NULL;
    }

    semaphore_P(mutex_client); // Acquire lock
    for (i = 0; i < NUMPORTS; i++) { // Loop once through entire array
        // The first time through the array, everything is null
        // so this should terminate in O(1) time
        if (client_ports[next_client_id] == NULL) { // Found new empty location
            cur_id = next_client_id;
            increment_client_id();
            new_client(cur_id, addr, port);

            semaphore_V(mutex_client); // Release lock

            client_handshake(client_ports[cur_id], error);
        }
        increment_client_id();
    }
    // All ports are taken
    semaphore_V(mutex_client); // Release lock

    *error = SOCKET_NOMOREPORTS;
    return NULL;
}


/*
 * Send a message to the other end of the socket.
 *
 * The send call should block until the remote host has ACKnowledged receipt of
 * the message.  This does not necessarily imply that the application has called
 * 'minisocket_receive', only that the packet is buffered pending a future
 * receive.
 *
 * It is expected that the order of calls to 'minisocket_send' implies the order
 * in which the concatenated messages will be received.
 *
 * 'minisocket_send' should block until the whole message is reliably
 * transmitted or an error/timeout occurs
 *
 * Arguments: the socket on which the communication is made (socket), the
 *            message to be transmitted (msg) and its length (len).
 * Return value: returns the number of successfully transmitted bytes. Sets the
 *               error code and returns -1 if an error is encountered.
 */
int
minisocket_send(minisocket_t socket, minimsg_t msg, int len, minisocket_error *error) {
return 0;
}

/*
 * Receive a message from the other end of the socket. Blocks until
 * some data is received (which can be smaller than max_len bytes).
 *
 * Arguments: the socket on which the communication is made (socket), the memory
 *            location where the received message is returned (msg) and its
 *            maximum length (max_len).
 * Return value: -1 in case of error and sets the error code, the number of
 *           bytes received otherwise
 */
int
minisocket_receive(minisocket_t socket, minimsg_t msg, int max_len, minisocket_error *error) {
return 0;
}

/* Close a connection. If minisocket_close is issued, any send or receive should
 * fail.  As soon as the other side knows about the close, it should fail any
 * send or receive in progress. The minisocket is destroyed by minisocket_close
 * function.  The function should never fail.
 */
void
minisocket_close(minisocket_t socket) {

}
