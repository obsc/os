/*
 *	Implementation of minisockets.
 */
#include "defs.h"
#include "minisocket.h"
#include "miniheader.h"
#include "network.h"
#include "queue.h"
#include "synch.h"

#define validServer(p) p >= 0 && p < NUMPORTS

struct minisocket {
    char port_type;
    int port_number;
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
return 0;
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
return 0;
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
