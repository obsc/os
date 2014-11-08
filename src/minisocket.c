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

enum { LISTEN = 1, SYN_RECEIVED, S_ESTABLISHED, S_CLOSING }; // Server state
enum { SYN_SENT = 1, C_ESTABLISHED, C_CLOSING }; // Client state
enum { SEND_ACK = 1, SEND_SENDING, SEND_CLOSE}; // Send state

struct minisocket {
    char socket_type;
    int port_number;
    // Remote connection
    network_address_t remote_address;
    int remote_port;

    int seq; // SEQ number
    int ack; // ACK number

    semaphore_t lock; // Lock on the minisocket
    
    // Control logic
    int transitioned; // boolean representing state of transition semaphore
    semaphore_t control_transition;

    // Send logic
    semaphore_t send_transition;
    int send_transition_count;
    semaphore_t send_lock;
    char send_state;

    // Receive logic

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

char *dummy; // Represents a dummy bytearray to pass to send pkt

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
    dummy = (char *) malloc(1);
    // Null check
    if ( !mutex_server || !mutex_client || !dummy ) {
        semaphore_destroy(mutex_server);
        semaphore_destroy(mutex_client);
        free(dummy);
        return;
    }

    semaphore_initialize(mutex_server, 1);
    semaphore_initialize(mutex_client, 1);
}

mini_header_reliable_t
create_header(minisocket_t socket, minisocket_error *error) {
    mini_header_reliable_t header;
    network_address_t my_address;

    header = (mini_header_reliable_t) malloc(sizeof(struct mini_header_reliable));
    if (!header) {
        *error = SOCKET_OUTOFMEMORY;
        return NULL;
    }

    header->protocol = PROTOCOL_MINISTREAM;

    network_get_my_address(my_address); // Get my address
    // Pack source and destination
    pack_address(header->source_address, my_address);
    pack_unsigned_short(header->source_port, socket->port_number);
    pack_address(header->destination_address, socket->remote_address);
    pack_unsigned_short(header->destination_port, socket->remote_port);
    // Pack seq and ack numbers
    pack_unsigned_int(header->seq_number, socket->seq);
    pack_unsigned_int(header->ack_number, socket->ack);

    *error = SOCKET_NOERROR;
    return header;
}

/*
 * Creates a new socket. Returns null upon failure
 */
minisocket_t
create_socket(int port) {
    minisocket_t socket = (minisocket_t) malloc (sizeof(struct minisocket));

    if ( !socket ) return NULL;

    // Generic port data
    socket->port_number = port;

    socket->seq = 1;
    socket->ack = 0;

    socket->send_transition_count = 0;

    socket->send_state = SEND_SENDING;

    socket->lock = semaphore_create();
    socket->control_transition = semaphore_create();
    socket->send_lock = semaphore_create();
    socket->send_transition = semaphore_create();
    if ( !socket->lock || !socket->control_transition || !socket->send_lock || !socket->send_transition) {
        free(lock);
        free(control_transition);
        free(send_lock);
        free(send_transition);
        free(socket);
        return NULL;
    }
    semaphore_initialize(socket->lock, 1);
    semaphore_initialize(socket->control_transition, 0);
    semaphore_initialize(socket->send_lock, 1);
    semaphore_initialize(socket->send_transition, 0);

    return socket;
}

/* Constructs a new server socket
 * Returns -1 upon memory failure
 */
int
new_server(int port) {
    minisocket_t socket = create_socket(port);
    if ( !socket ) return -1;

    // Socket type
    socket->socket_type = SERVER;
    // TODO: MORE STUFF
    socket->u.server.server_state = LISTEN;

    // Successfully created a server
    server_ports[port] = socket;
    return 0;
}

/* Constructs a new client socket
 * Returns -1 upon memory failure
 */
int
new_client(int client_id, network_address_t addr, int port) {
    minisocket_t socket = create_socket(client_id + NUMPORTS);
    if ( !socket ) return -1;

    // Socket type
    socket->socket_type = CLIENT;

    socket->remote_address[0] = addr[0];
    socket->remote_address[1] = addr[1];
    socket->remote_port = port;
    // TODO: MORE STUFF
    socket->u.client.client_state = SYN_SENT;

    // Successfully created a client
    client_ports[client_id] = socket;
    return 0;
}

void
control_reset(void *sock) {
    socket_t socket = (socket_t) sock;
    semaphore_P(socket->lock);
    if (socket->transitioned == 0) {
        socket->transitioned = 1;
        Semaphore_V(socket->control_transition);
    }
    semaphore_v(socket->lock);
}

/* Listens and blocks until connection successfully made with a client
 * Returns a minisocket upon success
 */
minisocket_t
server_handshake(minisocket_t socket, minisocket_error *error) {
    while (1) {
        switch (socket->u.server.server_state) {
            case LISTEN: // Listening for Syn
                break;
            case SYN_RECEIVED: // Sending Synacks
                break;
            case S_ESTABLISHED: // Received Ack
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
    mini_header_reliable_t header;
    int num_sent;
    int timeout;
    alarm_id retry_alarm;

    num_sent = 0;
    timeout = BASE_DELAY;
    retry_alarm = NULL;

    while (1) {
        switch (socket->u.client.client_state) {
            case SYN_SENT: // Sending Syn
                if (num_sent >= MAX_TIMEOUTS) { // Timeout too many times
                    *error = SOCKET_NOSERVER;
                    return NULL;
                }

                semaphore_P(socket->lock);
                header = create_header(socket, error); // Creates a header
                if (*error != SOCKET_NOERROR) return NULL;
                header->message_type = MSG_SYN; // Syn packet type

                network_send_pkt(socket->remote_address, sizeof(struct mini_header_reliable),
                                header, 0, dummy);

                free(header);
                retry_alarm = register_alarm(timeout, control_reset, socket);
                semaphore_V(socket->lock);

                num_sent++;
                timeout *= 2;

                semaphore_P(control_transition);
                deregister_alarm(retry_alarm);
                socket->transitioned = 0;
                break;
            case C_ESTABLISHED: // Received Synack
                *error = SOCKET_NOERROR;
                return socket;
            case C_CLOSING: // Socket closed
                *error = SOCKET_BUSY;
                return NULL;
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

void send_reset(void *sock) {
    minisocket_t socket = (minisocket_t) sock;
    semaphore_P(socket->lock);
    if (socket->send_transition_count == 0) {
        socket->send_transition_count = 1;
        semaphore_V(socket->send_transition);
    }
    semaphore_V(socket->lock);
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
    mini_header_reliable_t header;
    int size;
    interrupt_level_t old_level;
    int timeout;
    int num_sent;
    alarm_id retry_alarm;

    semaphore_P(socket->send_lock);

    if (!socket || !msg || !len) {
        *error = SOCKET_INVALIDPARAMS;
        return -1;
    }

    timeout = BASE_DELAY;
    num_sent = 0;
    semaphore_P(socket->lock);
    socket->seq = socket->seq + 1;
    semaphore_V(socket->lock);

    if (MAX_NETWORK_PKT_SIZE - sizeof(struct mini_header_reliable) < len) {
        size = MAX_NETWORK_PKT_SIZE - sizeof(struct mini_header_reliable);
    } else {
        size = len;
    }

    while (1) {
        switch (socket->send_state) {
            case SEND_ACK: 
                *error = SOCKET_NOERROR;
                semaphore_V(socket->send_lock);
                return size;
            case SEND_SENDING: 
                if (socket->num_sent >= 7) {
                    *error = SOCKET_SENDERROR;
                    semaphore_V(socket->send_lock);
                    return -1;
                }
                semaphore_P(socket->lock);
                header = create_header(socket, error);
                semaphore_V(socket->lock);
                if (!header) return -1;
                old_level = set_interrupt_level(DISABLED);
                if (network_send_pkt(socket->remote_address, sizeof(struct mini_header_reliable), (char *) header, size, msg) == -1) {
                    free(header);
                    *error = SOCKET_SENDERROR;
                    semaphore_V(socket->send_lock);
                    set_interrupt_level(old_level);
                    return -1;
                }
                retry_alarm = register_alarm(socket->timeout, send_reset, socket);                
                set_interrupt_level(old_level);
                semaphore_P(socket->send_transition);
                semaphore_P(socket->lock);
                socket->send_transition_count = 0;
                semaphore_V(socket->lock);
                deregister_alarm(retry_alarm);
                num_sent += 1;
                timeout = timeout * 2;
                free(header);
                break;
            case SEND_CLOSE:
                *error = SOCKET_SENDERROR;
                semaphore_V(socket->send_lock);
                return -1;
        }
    }
    return -1;


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
