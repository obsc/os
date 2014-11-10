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
#include "interrupts.h"
#include "stream.h"

enum { LISTEN = 1, SYN_RECEIVED, S_ESTABLISHED, S_CLOSING }; // Server state
enum { SYN_SENT = 1, C_ESTABLISHED, C_CLOSING }; // Client state
enum { SEND_ACK = 1, SEND_SENDING, SEND_CLOSE}; // Send state
enum { RECEIVE_RECEIVING = 1, RECEIVE_CLOSE}; // Receive state
enum { CLOSE_CLOSING = 1, CLOSE_ACK} // Close state

struct minisocket {
    char socket_type;
    int port_number;
    // Remote connection
    network_address_t remote_address;
    int remote_port;

    int seq; // SEQ number
    int ack; // ACK number

    semaphore_t lock; // Lock on the minisocket

    stream_t stream; // Stream on this socket
    
    // Control logic
    int transitioned; // boolean representing state of transition semaphore
    semaphore_t control_transition;

    // Send logic
    semaphore_t send_transition;
    int send_transition_count;
    semaphore_t send_lock;
    char send_state;

    // Receive logic
    char receive_state;
    semaphore_t received_data;
    int receive_waiting_count;

    // Close logic
    int closing;
    char close_state;
    

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

void
socket_transition(minisocket_t socket) {
    if (socket->transitioned == 0) {
        socket->transitioned = 1;
        semaphore_V(socket->control_transition);
    }
}

void
control_reset(void *sock) {
    minisocket_t socket = (minisocket_t) sock;
    semaphore_P(socket->lock);
    socket_transition(socket);
    semaphore_V(socket->lock);
}

/*
 * Returns a reliable header
 */
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
 * Replies to remote with current socket state (empty message)
 */
void
reply(minisocket_t socket, char message_type) {
    minisocket_error err;
    mini_header_reliable_t header;

    header = create_header(socket, &err);
    if ( err == SOCKET_NOERROR ) {
        header->message_type = message_type;
        network_send_pkt(socket->remote_address, sizeof(struct mini_header_reliable), (char *) header, 0, dummy);
        free(header);
    }
}

/*
 * Handles Syn packets (only server)
 */
void
handle_syn(minisocket_t socket, network_address_t source, int source_port) {
    switch (socket->u.server.server_state) {
        // Syn to listening port
        case LISTEN:
            socket->u.server.server_state = SYN_RECEIVED;
            socket->remote_address[0] = source[0];
            socket->remote_address[1] = source[1];
            socket->remote_port = source_port;
    
            socket_transition(socket);
            return;
        // Syn to busy port
        case SYN_RECEIVED:
        case S_ESTABLISHED:
            // Source validation
            if ( socket->remote_port != source_port ||
                 !network_compare_network_addresses(socket->remote_address, source) ) {
                reply(socket, MSG_FIN); // Port busy
                return;
            }
            return;
        // Drops Syn packet otherwise
        default:
            return;
    }   
}

/*
 * Handles Synack packets (only client)
 */
void
handle_synack(minisocket_t socket, network_address_t source, int source_port) {
    // Source validation
    if ( socket->remote_port != source_port ||
         !network_compare_network_addresses(socket->remote_address, source) ) {
        return;
    }

    if (socket->u.client.client_state == SYN_SENT) {
        socket->u.client.client_state = C_ESTABLISHED;
        if (socket->ack == 0) socket->ack = 1;
        socket_transition(socket);
    }

    reply(socket, MSG_ACK); // Ack the packet
}

/*
 * Handles ack packets
 */
void
handle_ack(minisocket_t socket, network_address_t source, int source_port, int ack, int seq, network_interrupt_arg_t *arg) {
    // Source validation
    if ( socket->remote_port != source_port ||
         !network_compare_network_addresses(socket->remote_address, source) ) {
        return;
    }

    // Waiting for ack on synack
    if (socket->socket_type == SERVER && socket->u.server.server_state == SYN_RECEIVED) {
        if (ack == 1) { // Drop non-ack 1 packets
            socket->u.server.server_state = S_ESTABLISHED;
            socket_transition(socket);
        }
    }

    if (ack == socket->seq && ack > 1) {
        if (socket->send_state == SEND_SENDING) {
            socket->send_state = SEND_ACK;
            if (socket->send_transition_count != 1) {
                semaphore_V(socket->send_transition);  
            }
            socket->send_transition_count = 1;
        }
    }
    if (arg->size > sizeof(struct mini_header_reliable)) {
        if (seq - 1 == socket->ack) {
            socket->ack += 1;
            stream_add(socket->stream, arg);
            semaphore_V(socket->received_data);
        }
        reply(socket, MSG_ACK);
    }
}

/*
 * Handles fin packets
 */
void
handle_fin(minisocket_t socket, network_address_t source, int source_port) {
    // Source validation
    if ( socket->remote_port != source_port ||
         !network_compare_network_addresses(socket->remote_address, source) ) {
        return;
    }
}


/*
 * Handler for receiving a message on a socket
 * Assumes interrupts are disabled throughout
 */
void
minisocket_handle(network_interrupt_arg_t *arg) {
    mini_header_reliable_t header;
    network_address_t source;
    int source_port;
    int port;
    minisocket_t socket;
    int seq;
    int ack;

    header = (mini_header_reliable_t) arg->buffer;
    unpack_address(header->source_address, source);
    source_port = unpack_unsigned_short(header->source_port);
    port = unpack_unsigned_short(header->destination_port);
    seq = unpack_unsigned_int(header->seq_number);
    ack = unpack_unsigned_int(header->ack_number);

    if ( port >= 0 && port < NUMPORTS ) { // Server port
        if ( !server_ports[port] ) return;
        socket = server_ports[port];
    } else if ( port >= NUMPORTS && port < 2 * NUMPORTS ) { // Client port
        if ( !client_ports[port - NUMPORTS] ) return;
        socket = client_ports[port -  NUMPORTS];
    } else {
        return;
    }

    switch (header->message_type) {
        case MSG_SYN:
            if (socket->socket_type == SERVER)
                handle_syn(socket, source, source_port);
            break;
        case MSG_SYNACK:
            if (socket->socket_type == CLIENT)
                handle_synack(socket, source, source_port);
            break;
        case MSG_ACK:
            handle_ack(socket, source, source_port, ack, seq, arg);
            break;
        case MSG_FIN:
            handle_fin(socket, source, source_port);
            break;
    }

    if (!(header->message_type == MSG_ACK && arg->size > sizeof(struct mini_header_reliable))) {
        free(arg);
    }
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

    socket->receive_state = RECEIVE_RECEIVING;

    socket->receive_waiting_count = 0;

    socket->received_data = semaphore_create();

    socket->lock = semaphore_create();

    socket->transitioned = 0;
    socket->control_transition = semaphore_create();
    socket->send_lock = semaphore_create();
    socket->send_transition = semaphore_create();

    socket->stream = stream_new();
    if ( !socket->lock || !socket->control_transition ||
         !socket->send_lock || !socket->send_transition ||
         !socket->received_data || !socket->stream ) {
        free(socket->lock);
        free(socket->control_transition);
        free(socket->send_lock);
        free(socket->send_transition);
        free(socket->received_data);
        free(socket->stream);
        free(socket);
        return NULL;
    }
    semaphore_initialize(socket->lock, 1);
    semaphore_initialize(socket->control_transition, 0);
    semaphore_initialize(socket->send_lock, 1);
    semaphore_initialize(socket->send_transition, 0);
    semaphore_initialize(socket->received_data, 0);

    return socket;
}

/*
 * Destroys a socket
 */
void
destroy_socket(minisocket_t socket) {
    free(socket->lock);
    free(socket->control_transition);
    free(socket->send_lock);
    free(socket->send_transition);
    free(socket->received_data);
    free(socket->stream);
    free(socket);
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

/* Listens and blocks until connection successfully made with a client
 * Returns a minisocket upon success, otherwise NULL
 */
minisocket_t
server_handshake(minisocket_t socket, minisocket_error *error) {
    mini_header_reliable_t header;
    int num_sent;
    int timeout;
    alarm_id retry_alarm;

    while (1) {
        switch (socket->u.server.server_state) {
            case LISTEN: // Listening for Syn
                num_sent = 0;
                timeout = BASE_DELAY;
                retry_alarm = NULL;
                semaphore_P(socket->control_transition);
                socket->ack = 1;
                socket->transitioned = 0;
                break;
            case SYN_RECEIVED: // Sending Synacks
                if (num_sent >= MAX_TIMEOUTS) { // Timeout too many times
                    socket->ack = 0;
                    socket->u.server.server_state = LISTEN;
                    break;
                }

                semaphore_P(socket->lock);
                header = create_header(socket, error); // Creates a header
                if (*error != SOCKET_NOERROR) return NULL;
                header->message_type = MSG_SYNACK; // Synack packet type

                network_send_pkt(socket->remote_address, sizeof(struct mini_header_reliable), (char *) header, 0, dummy);

                free(header);
                retry_alarm = register_alarm(timeout, control_reset, socket); // Set up alarm
                semaphore_V(socket->lock);

                num_sent++;
                timeout *= 2;

                semaphore_P(socket->control_transition);
                deregister_alarm(retry_alarm);
                socket->transitioned = 0;
                break;
            case S_ESTABLISHED: // Received Ack
                *error = SOCKET_NOERROR;
                return socket;
            case S_CLOSING: // Socket closed, this should not happen in here
                return NULL;
        }
    }
}

/* Tries to connect to a server
 * Returns a minisocket upon success, otherwise NULL
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

                network_send_pkt(socket->remote_address, sizeof(struct mini_header_reliable), (char *) header, 0, dummy);

                free(header);
                retry_alarm = register_alarm(timeout, control_reset, socket); // Set up alarm
                semaphore_V(socket->lock);

                num_sent++;
                timeout *= 2;

                semaphore_P(socket->control_transition);
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
    minisocket_t socket;

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

    socket = server_handshake(server_ports[port], error);

    if ( !socket ) { // Handshake failed
        semaphore_P(mutex_server);
        destroy_socket(server_ports[port]);
        server_ports[port] = NULL;
        semaphore_V(mutex_server);
        return NULL;
    } else {
        return socket;
    }
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
    minisocket_t socket;

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

            socket = client_handshake(client_ports[cur_id], error);
            if ( !socket ) { // Handshake failed
                semaphore_P(mutex_client);
                destroy_socket(client_ports[cur_id]);
                client_ports[cur_id] = NULL;
                semaphore_V(mutex_client);
                return NULL;
            } else {
                return socket; 
            }
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
    int message_iterator;
    int len_left;

    // if length is 0, send nothing and return
    if (len == 0) {
        *error = SOCKET_NOERROR;
        return 0;
    }

    if (!error) return -1;
    if (!socket || !msg) {
        *error = SOCKET_INVALIDPARAMS;
        return -1;
    }

    semaphore_P(socket->lock);
    if (socket->closing) {
        *error = SOCKET_SENDERROR;
        semaphore_V(socket->lock);
        return -1;
    }
    semaphore_V(socket->lock);

    // only 1 send at any given time
    semaphore_P(socket->send_lock);

    // initialize values
    timeout = BASE_DELAY;
    num_sent = 0;
    len_left = len;
    message_iterator = 0;
    semaphore_P(socket->lock);
    socket->seq++;
    semaphore_V(socket->lock);

    // iterate until no more data to send
    while (len_left > 0) {

        // find the size for this iteration of send
        if (MAX_NETWORK_PKT_SIZE - sizeof(struct mini_header_reliable) < len_left) {
            size = MAX_NETWORK_PKT_SIZE - sizeof(struct mini_header_reliable);
        } else {
            size = len_left;
        }

        switch (socket->send_state) {
            // received ack, send successful
            case SEND_ACK:
                message_iterator += size;
                len_left -= size;
                num_sent = 0;
                timeout = BASE_DELAY;
                socket->send_state = SEND_SENDING;
                // increase seq if there are data left to send
                if (len_left > 0) socket->seq++;
                break;
            // attempts to send current chunk
            case SEND_SENDING:
                if (num_sent >= MAX_TIMEOUTS) {
                    *error = SOCKET_SENDERROR;
                    semaphore_V(socket->send_lock);
                    if (message_iterator == 0) {
                        return -1;
                    } else {
                        return message_iterator;
                    }
                }

                // create header
                semaphore_P(socket->lock);
                header = create_header(socket, error);
                if (!header) {
                    semaphore_V(socket->lock);
                    if (message_iterator == 0) {
                        return -1;
                    } else {
                        return message_iterator;
                    }
                }
                header->message_type = MSG_ACK;
                // create alarm to timeout
                network_send_pkt(socket->remote_address, sizeof(struct mini_header_reliable), (char *) header, size, msg+message_iterator);
                retry_alarm = register_alarm(timeout, send_reset, socket);
                semaphore_V(socket->lock);

                // wait for ack response or timeout signal
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
                if (message_iterator == 0) {
                    return -1;
                } else {
                    return message_iterator;
                }
        }
    }
    semaphore_V(socket->send_lock);
    return message_iterator;


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
    int output;

    if (!error) return -1;
    if (!socket || !msg) {
        *error = SOCKET_INVALIDPARAMS;
        return -1;
    }

    // is waiting
    semaphore_P(socket->lock);
    socket->receive_waiting_count += 1;
    if (socket->closing) {
        *error = SOCKET_RECEIVEERROR;
        semaphore_V(socket->lock);
        return -1;
    }
    semaphore_V(socket->lock);

    // wait until there is data
    semaphore_P(socket->received_data);

    switch (socket->receive_state) {
        case RECEIVE_RECEIVING:
            output = stream_take(socket->stream, max_len, msg);

            // set error based on if there is output
            if (output != -1) {
                *error = SOCKET_NOERROR;
            }
            *error = SOCKET_RECEIVEERROR;

            // indicate if there is data left over
            if (stream_is_empty(socket->stream) == 0) {
                semaphore_V(socket->received_data);
            }

            // no longer waiting
            semaphore_P(socket->lock);
            socket->receive_waiting_count -= 1;
            semaphore_V(socket->lock);

            return output;
        case RECEIVE_CLOSE:
            *error = SOCKET_RECEIVEERROR;
            return -1;
    }
    return -1;
}

/* Close a connection. If minisocket_close is issued, any send or receive should
 * fail.  As soon as the other side knows about the close, it should fail any
 * send or receive in progress. The minisocket is destroyed by minisocket_close
 * function.  The function should never fail.
 */
void
minisocket_close(minisocket_t socket) {

}
