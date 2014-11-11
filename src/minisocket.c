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
#include "state.h"

#define CLOSEDELAY 15000

enum { LISTEN = 1, SYN_RECEIVED, S_ESTABLISHED }; // Server state
enum { SYN_SENT = 1, C_ESTABLISHED, C_CLOSING }; // Client state
enum { SEND_SENDING = 1, SEND_ACK, SEND_CLOSE}; // Send state
enum { RECEIVE_RECEIVING = 1, RECEIVE_CLOSE}; // Receive state
enum { OPEN = 1, CLOSING , CLOSED }; // Close state

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

    // Send logic
    state_t send_state;
    semaphore_t send_lock;
    int send_waiting_count;

    // Receive logic
    char receive_state;
    int receive_waiting_count;
    semaphore_t receive_lock;

    // Close logic
    state_t close_state;
    alarm_id close_alarm;
    int fin_seq;

    union {
        struct {
            state_t server_state;
        } server;
        struct {
            state_t client_state;
        } client;
    } u;
};

semaphore_t mutex_server; // Lock for server sockets
semaphore_t mutex_client; // Lock for client sockets

minisocket_t server_ports[NUMPORTS]; // Array of all the server sockets
minisocket_t client_ports[NUMPORTS]; // Array of all the client sockets
int next_client_id; // Next client port id to use

char *dummy; // Represents a dummy bytearray to pass to send pkt

void end_send_receive(void *sock);

/*
 * Returns a reliable header to a destination
 */
mini_header_reliable_t
create_header_to_address(minisocket_t socket, network_address_t dest, int dest_port, minisocket_error *error) {
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
    pack_address(header->destination_address, dest);
    pack_unsigned_short(header->destination_port, dest_port);
    // Pack seq and ack numbers
    pack_unsigned_int(header->seq_number, socket->seq);
    pack_unsigned_int(header->ack_number, socket->ack);

    *error = SOCKET_NOERROR;
    return header;
}

/*
 * Returns a reliable header
 */
mini_header_reliable_t
create_header(minisocket_t socket, minisocket_error *error) {
    return create_header_to_address(socket, socket->remote_address, socket->remote_port, error);
}

/*
 * Replies to address
 */
void
reply_to_address(minisocket_t socket, char message_type, network_address_t dest, int dest_port) {
    minisocket_error err;
    mini_header_reliable_t header;

    header = create_header_to_address(socket, dest, dest_port, &err);
    if ( err == SOCKET_NOERROR ) {
        header->message_type = message_type;
        network_send_pkt(dest, sizeof(struct mini_header_reliable), (char *) header, 0, dummy);
        free(header);
    }
}

/*
 * Replies to remote with current socket state (empty message)
 */
void
reply(minisocket_t socket, char message_type) {
    reply_to_address(socket, message_type, socket->remote_address, socket->remote_port);
}

/*
 * Handles Syn packets (only server)
 */
void
handle_syn(minisocket_t socket, network_address_t source, int source_port) {
    //printf("syn\n");
    switch (get_state(socket->u.server.server_state)) {
        // Syn to listening port
        case LISTEN:
            socket->remote_address[0] = source[0];
            socket->remote_address[1] = source[1];
            socket->remote_port = source_port;

            transition_to(socket->u.server.server_state, SYN_RECEIVED);
            return;
        // Syn to busy port
        case SYN_RECEIVED:
        case S_ESTABLISHED:
            // Source validation
            if ( socket->remote_port != source_port ||
                 !network_compare_network_addresses(socket->remote_address, source) ) {
                reply_to_address(socket, MSG_FIN, source, source_port); // Port busy
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
    //printf("synack\n");
    // Source validation
    if ( socket->remote_port != source_port ||
         !network_compare_network_addresses(socket->remote_address, source) ) {
        return;
    }

    if (get_state(socket->u.client.client_state) == SYN_SENT) {
        if (socket->ack == 0) socket->ack = 1;
        transition_to(socket->u.client.client_state, C_ESTABLISHED);
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
    if (socket->socket_type == SERVER && get_state(socket->u.server.server_state) == SYN_RECEIVED) {
        //printf("synackack\n");
        if (ack == 1) { // Drop non-ack 1 packets
            transition_to(socket->u.server.server_state, S_ESTABLISHED);
        }
    }

    // Waiting for ack on fin
    if (get_state(socket->close_state) == CLOSING && ack >= socket->fin_seq) {
        //printf("finack\n");
        transition_to(socket->close_state, CLOSED);
    }

    // Waiting for ack on data
    else if (ack == socket->seq && ack > 1) {
        //printf("dataack\n");
        if (get_state(socket->send_state) == SEND_SENDING) {
            transition_to(socket->send_state, SEND_ACK);
        }
    }
    // Data
    else if (arg->size > sizeof(struct mini_header_reliable)) {
        //printf("data\n");
        if (seq - 1 == socket->ack && socket->receive_state == RECEIVE_RECEIVING) {
            socket->ack++;
            stream_add(socket->stream, arg);
            semaphore_V(socket->receive_lock);
        }
        reply(socket, MSG_ACK);
    }
}


/*
 * Handles fin packets
 */
void
handle_fin(minisocket_t socket, network_address_t source, int source_port, int seq) {
    //printf("fin\n");
    // Source validation
    if ( socket->remote_port != source_port ||
         !network_compare_network_addresses(socket->remote_address, source) ) {
        return;
    }

    if ( socket->socket_type == CLIENT && get_state(socket->u.client.client_state) == SYN_SENT ) {
        transition_to(socket->u.client.client_state, C_CLOSING);
    }

    if ( (socket->socket_type == CLIENT && get_state(socket->u.client.client_state) == C_ESTABLISHED) ||
         (socket->socket_type == SERVER && get_state(socket->u.server.server_state) == S_ESTABLISHED) ) {
        if (get_state(socket->close_state) == OPEN) {
            transition_to(socket->close_state, CLOSING);
            socket->close_alarm = register_alarm(CLOSEDELAY, end_send_receive, socket);
        }
        if (socket->ack < seq) socket->ack = seq;
        reply(socket, MSG_ACK);
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

    if (get_state(socket->close_state) == CLOSED) return; // Already closed

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
            handle_fin(socket, source, source_port, seq);
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

    socket->lock = semaphore_create();

    socket->stream = stream_new();

    socket->send_state = state_new(SEND_SENDING);
    socket->send_lock = semaphore_create();
    socket->send_waiting_count = 0;

    socket->receive_state = RECEIVE_RECEIVING;
    socket->receive_waiting_count = 0;
    socket->receive_lock = semaphore_create();

    socket->close_state = state_new(OPEN);
    socket->close_alarm = NULL;
    socket->fin_seq = 0;

    if ( !socket->lock || !socket->send_lock || !socket->send_state ||
         !socket->receive_lock || !socket->stream ||
         !socket->close_state ) {
        semaphore_destroy(socket->lock);
        semaphore_destroy(socket->send_lock);
        semaphore_destroy(socket->receive_lock);
        state_destroy(socket->close_state);
        state_destroy(socket->send_state);
        stream_destroy(socket->stream);
        free(socket);
        return NULL;
    }
    semaphore_initialize(socket->lock, 1);
    semaphore_initialize(socket->send_lock, 1);
    semaphore_initialize(socket->receive_lock, 0);

    return socket;
}

/*
 * Clears and destroys a socket from arrays
 */
void
destroy_socket(minisocket_t socket) {
    if (socket->socket_type == SERVER) {
        semaphore_P(mutex_server);
        server_ports[socket->port_number] = NULL;
        semaphore_V(mutex_server);
    } else {
        semaphore_P(mutex_client);
        client_ports[socket->port_number - NUMPORTS] = NULL;
        semaphore_V(mutex_client);
    }
    semaphore_destroy(socket->lock);
    semaphore_destroy(socket->send_lock);
    semaphore_destroy(socket->receive_lock);
    state_destroy(socket->send_state);
    state_destroy(socket->close_state);
    stream_destroy(socket->stream);
    if (socket->socket_type == SERVER)
        state_destroy(socket->u.server.server_state);
    else
        state_destroy(socket->u.client.client_state);
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

    socket->u.server.server_state = state_new(LISTEN);

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

    socket->u.client.client_state = state_new(SYN_SENT);

    // Successfully created a client
    client_ports[client_id] = socket;
    return 0;
}

/* Listens and blocks until connection successfully made with a client
 * Returns a minisocket upon success, otherwise NULL
 */
minisocket_t
server_handshake(minisocket_t socket, minisocket_error *error) {
    int num_sent;
    int timeout;
    alarm_id retry_alarm;

    while (1) {
        switch (get_state(socket->u.server.server_state)) {
            case LISTEN: // Listening for Syn
                num_sent = 0;
                timeout = BASE_DELAY;
                retry_alarm = NULL;
                wait_for_transition(socket->u.server.server_state);
                socket->ack = 1;
                break;
            case SYN_RECEIVED: // Sending Synacks
                if (num_sent >= MAX_TIMEOUTS) { // Timeout too many times
                    socket->ack = 0;
                    set_state(socket->u.server.server_state, LISTEN);
                    break;
                }

                semaphore_P(socket->lock);
                reply(socket, MSG_SYNACK);

                retry_alarm = register_alarm(timeout, transition_timer, socket->u.server.server_state); // Set up alarm
                semaphore_V(socket->lock);

                num_sent++;
                timeout *= 2;

                wait_for_transition(socket->u.server.server_state);
                deregister_alarm(retry_alarm);
                break;
            case S_ESTABLISHED: // Received Ack
                *error = SOCKET_NOERROR;
                return socket;
        }
    }
}

/* Tries to connect to a server
 * Returns a minisocket upon success, otherwise NULL
 */
minisocket_t
client_handshake(minisocket_t socket, minisocket_error *error) {
    int num_sent;
    int timeout;
    alarm_id retry_alarm;

    num_sent = 0;
    timeout = BASE_DELAY;
    retry_alarm = NULL;

    while (1) {
        switch (get_state(socket->u.client.client_state)) {
            case SYN_SENT: // Sending Syn
                if (num_sent >= MAX_TIMEOUTS) { // Timeout too many times
                    *error = SOCKET_NOSERVER;
                    return NULL;
                }

                semaphore_P(socket->lock);
                reply(socket, MSG_SYN);

                retry_alarm = register_alarm(timeout, transition_timer, socket->u.client.client_state); // Set up alarm
                semaphore_V(socket->lock);

                num_sent++;
                timeout *= 2;

                wait_for_transition(socket->u.client.client_state);
                deregister_alarm(retry_alarm);
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

    if ( !error ) return NULL;

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
        destroy_socket(server_ports[port]);
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

    if ( !error ) return NULL;

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
                destroy_socket(client_ports[cur_id]);
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

/* Checks if this thread is the last sending and receiving thread
 * and if we are currently in the closed state
 * Frees all data structures if it is
 */
void
check_last(minisocket_t socket) {
    if (get_state(socket->close_state) == CLOSED) {
        if (socket->receive_waiting_count == 0 && socket->send_waiting_count == 0) {
            transition(socket->close_state);
            destroy_socket(socket);
        }
    }
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
    int timeout;
    int num_sent;
    alarm_id retry_alarm;
    int message_iterator;
    int len_left;

    // Sanity checks
    if (!error) return -1;
    if (!socket || !msg) {
        *error = SOCKET_INVALIDPARAMS;
        return -1;
    }

    // if length is 0, send nothing and return
    if (len == 0) {
        *error = SOCKET_NOERROR;
        return 0;
    }

    semaphore_P(socket->lock);
    // Socket closing, no new threads
    if (get_state(socket->close_state) != OPEN) {
        *error = SOCKET_SENDERROR;
        semaphore_V(socket->lock);
        return -1;
    }
    socket->send_waiting_count += 1;
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

        switch (get_state(socket->send_state)) {
            // attempts to send current chunk
            case SEND_SENDING:
                if (num_sent >= MAX_TIMEOUTS) {
                    *error = SOCKET_SENDERROR;
                    semaphore_V(socket->send_lock);
                    semaphore_P(socket->lock);
                    socket->send_waiting_count -= 1;
                    semaphore_V(socket->lock);

                    check_last(socket);
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
                    socket->send_waiting_count -= 1;
                    semaphore_V(socket->lock);

                    check_last(socket);
                    if (message_iterator == 0) {
                        return -1;
                    } else {
                        return message_iterator;
                    }
                }
                header->message_type = MSG_ACK;
                // create alarm to timeout
                network_send_pkt(socket->remote_address, sizeof(struct mini_header_reliable), (char *) header, size, msg+message_iterator);
                free(header);

                retry_alarm = register_alarm(timeout, transition_timer, socket->send_state);
                semaphore_V(socket->lock);

                num_sent++;
                timeout *= 2;

                wait_for_transition(socket->send_state);
                deregister_alarm(retry_alarm);
                break;
            // received ack, send successful
            case SEND_ACK:
                message_iterator += size;
                len_left -= size;
                num_sent = 0;
                timeout = BASE_DELAY;
                set_state(socket->send_state, SEND_SENDING);
                // increase seq if there are data left to send
                if (len_left > 0) socket->seq++;
                break;
            case SEND_CLOSE:
                *error = SOCKET_SENDERROR;
                semaphore_P(socket->lock);
                socket->send_waiting_count -= 1;
                semaphore_V(socket->lock);
                semaphore_V(socket->send_lock);

                check_last(socket);
                if (message_iterator == 0) {
                    return -1;
                } else {
                    return message_iterator;
                }
        }
    }

    semaphore_V(socket->send_lock);
    semaphore_P(socket->lock);
    socket->send_waiting_count -= 1;
    semaphore_V(socket->lock);

    check_last(socket);
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
    // Sanity checks
    if (!error) return -1;
    if (!socket || !msg) {
        *error = SOCKET_INVALIDPARAMS;
        return -1;
    }

    // if length is 0, receive nothing and return
    if (max_len == 0) {
        *error = SOCKET_NOERROR;
        return 0;
    }

    // is waiting
    semaphore_P(socket->lock);
    // Socket closing, no new threads
    if (get_state(socket->close_state) != OPEN) {
        *error = SOCKET_RECEIVEERROR;
        semaphore_V(socket->lock);
        return -1;
    }
    socket->receive_waiting_count += 1;
    semaphore_V(socket->lock);

    // wait until there is data
    semaphore_P(socket->receive_lock);

    switch (socket->receive_state) {
        case RECEIVE_RECEIVING:
            output = stream_take(socket->stream, max_len, msg);

            // set error based on if there is output
            if (output != -1) {
                *error = SOCKET_NOERROR;
            }
            *error = SOCKET_RECEIVEERROR;

            // no longer waiting
            semaphore_P(socket->lock);
            socket->receive_waiting_count -= 1;
            semaphore_V(socket->lock);

            // indicate if there is data left over
            if (stream_is_empty(socket->stream) == 0 || get_state(socket->close_state) == CLOSED) {
                semaphore_V(socket->receive_lock);
            }

            check_last(socket);
            return output;
        case RECEIVE_CLOSE:
            *error = SOCKET_RECEIVEERROR;
            semaphore_P(socket->lock);
            socket->receive_waiting_count -= 1;
            semaphore_V(socket->lock);
            semaphore_V(socket->receive_lock);

            check_last(socket);
            return -1;
    }
    semaphore_P(socket->lock);
    socket->receive_waiting_count -= 1;
    semaphore_V(socket->lock);

    check_last(socket);
    return -1;
}

/* terminates receives and sends */
void end_send_receive(void *sock) {
    minisocket_t socket;

    socket = (minisocket_t) sock;

    // Terminates sends
    transition_to(socket->send_state, SEND_CLOSE);
    semaphore_V(socket->send_lock);
    // Terminates receives
    socket->receive_state = RECEIVE_CLOSE;
    semaphore_V(socket->receive_lock);

    // If there are no sends or receives running, terminate
    check_last(socket);
}

/* Close a connection. If minisocket_close is issued, any send or receive should
 * fail.  As soon as the other side knows about the close, it should fail any
 * send or receive in progress. The minisocket is destroyed by minisocket_close
 * function.  The function should never fail.
 */
void
minisocket_close(minisocket_t socket) {
    int num_sent;
    int timeout;
    alarm_id retry_alarm;

    if (get_state(socket->close_state) == CLOSING) {
        wait_for_transition(socket->close_state);
        return;
    } else if (get_state(socket->close_state) == CLOSED) {
        return;
    }

    timeout = BASE_DELAY;
    num_sent = 0;
    semaphore_P(socket->lock);
    socket->seq++;
    socket->fin_seq = socket->seq;
    set_state(socket->close_state, CLOSING);
    semaphore_V(socket->lock);

    while (1) {
        switch (get_state(socket->close_state)) {
            case OPEN:
                wait_for_transition(socket->close_state);
            // attempts to send fin packet
            case CLOSING:
                if (num_sent >= MAX_TIMEOUTS) {
                    set_state(socket->close_state, CLOSED);
                    break;
                }

                // create header
                semaphore_P(socket->lock);
                reply(socket, MSG_FIN);
                // create alarm to timeout
                retry_alarm = register_alarm(timeout, transition_timer, socket->close_state);
                semaphore_V(socket->lock);

                num_sent++;
                timeout *= 2;

                // wait for ack response or timeout signal
                wait_for_transition(socket->close_state);
                deregister_alarm(retry_alarm);
                break;
            // received ack, considered closed
            case CLOSED:
                end_send_receive(socket);
                wait_for_transition(socket->close_state);
                return;
        }
    }
}
