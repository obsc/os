/*
 *  Implementation of minimsgs and miniports.
 */
#include "defs.h"
#include "minimsg.h"
#include "miniheader.h"
#include "network.h"
#include "queue.h"
#include "synch.h"

#define validUnbound(p) p >= 0 && p < NUMPORTS

// Miniport structure
struct miniport {
    char port_type;
    int port_number;

    union {
        struct {
            queue_t incoming_data;
            semaphore_t lock;
            semaphore_t ready;
        } unbound;
        struct {
            network_address_t remote_address;
            int remote_unbound_port;
        } bound;
    } u;
};

semaphore_t mutex_unbound; // Lock for unbound ports
semaphore_t mutex_bound; // Lock for bound ports

miniport_t unbound_ports[NUMPORTS]; // Array of all the unbound ports
miniport_t bound_ports[NUMPORTS]; // Array of all the bound ports
int next_bound_id; // Next bound port id to use (NUMPORTS less than port number)

/*
 * Handler for receiving a minimsg
 * Assumes interrupts are disabled throughout
 */
void
minimsg_handle(network_interrupt_arg_t *arg) {
    mini_header_t header;
    int source;
    int destination;

    header = (mini_header_t) arg->buffer;
    source = unpack_unsigned_short(header->source_port);
    destination = unpack_unsigned_short(header->destination_port);

    // Sanity checks
    if (!(validUnbound(source)) || !(validUnbound(destination)) ||
        unbound_ports[destination] == NULL) {
        return;
    }

    // Lock incoming data queue
    queue_append(unbound_ports[destination]->u.unbound.incoming_data, arg);
    // One more piece of data ready
    semaphore_V(unbound_ports[destination]->u.unbound.ready);
}

/* Increments the bound id
 */
void
increment_bound_id() {
    if (++next_bound_id >= NUMPORTS) {
        next_bound_id = 0;
    }
}

/* performs any required initialization of the minimsg layer.
 */
void
minimsg_initialize() {
    int i;
    // Initialize global id counter
    next_bound_id = 0;
    // Initializes both arrays to NULL
    for (i = 0; i < NUMPORTS; i++) {
        unbound_ports[i] = NULL;
        bound_ports[i] = NULL;
    }
    // Initialize mutices
    mutex_unbound = semaphore_create();
    mutex_bound = semaphore_create();
    // Null check
    if ( !mutex_unbound || !mutex_bound ) {
        semaphore_destroy(mutex_unbound);
        semaphore_destroy(mutex_bound);
        return;
    }

    semaphore_initialize(mutex_unbound, 1);
    semaphore_initialize(mutex_bound, 1);
}

/*
 * Instantiates a new unbound port and puts it into the array of ports
 * Should only be called when the port is NULL
 */
void
new_unbound(int port_number) {
    miniport_t port = (miniport_t) malloc (sizeof(struct miniport));

    if ( !port ) return;

    // Generic port data
    port->port_type = UNBOUND;
    port->port_number = port_number;
    // Unbound port data
    port->u.unbound.incoming_data = queue_new();
    port->u.unbound.lock = semaphore_create();
    port->u.unbound.ready = semaphore_create();
    // Null check (free if any mallocs fail)
    if ( !port->u.unbound.incoming_data ||
         !port->u.unbound.lock || !port->u.unbound.ready) {
        queue_free(port->u.unbound.incoming_data);
        semaphore_destroy(port->u.unbound.lock);
        semaphore_destroy(port->u.unbound.ready);
        free(port);
        return;
    }

    semaphore_initialize(port->u.unbound.lock, 1);
    semaphore_initialize(port->u.unbound.ready, 0);

    // Successfully created an unbound port
    unbound_ports[port_number] = port;
}

/*
 * Instantiates a new bound port and puts it into the array of ports
 * Should only be called when the port is NULL
 */
void
new_bound(int bound_id, network_address_t addr, int remote_unbound_port) {
    miniport_t port = (miniport_t) malloc (sizeof(struct miniport));

    if ( !port ) return;

    // Generic port data
    port->port_type = BOUND;
    port->port_number = bound_id + NUMPORTS;
    // Bound port data
    port->u.bound.remote_address[0] = addr[0];
    port->u.bound.remote_address[1] = addr[1];
    port->u.bound.remote_unbound_port = remote_unbound_port;

    // Successfully created a bound port
    bound_ports[bound_id] = port;
}

/* Creates an unbound port for listening. Multiple requests to create the same
 * unbound port should return the same miniport reference. It is the responsibility
 * of the programmer to make sure he does not destroy unbound miniports while they
 * are still in use by other threads -- this would result in undefined behavior.
 * Unbound ports must range from 0 to 32767. If the programmer specifies a port number
 * outside this range, it is considered an error.
 */
miniport_t
miniport_create_unbound(int port_number) {
    // Out of range check
    if (port_number < 0 || port_number >= NUMPORTS) return NULL;

    semaphore_P(mutex_unbound); // Acquire lock
    if (unbound_ports[port_number] == NULL) {
        new_unbound(port_number); // Create new port if port did not exist
    }
    semaphore_V(mutex_unbound); // Release lock

    return unbound_ports[port_number];
}

/* Creates a bound port for use in sending packets. The two parameters, addr and
 * remote_unbound_port_number together specify the remote's listening endpoint.
 * This function should assign bound port numbers incrementally between the range
 * 32768 to 65535. Port numbers should not be reused even if they have been destroyed,
 * unless an overflow occurs (ie. going over the 65535 limit) in which case you should
 * wrap around to 32768 again, incrementally assigning port numbers that are not
 * currently in use.
 */
miniport_t
miniport_create_bound(network_address_t addr, int remote_unbound_port_number) {
    int i;
    int cur_id;

    // Out of range check
    if (remote_unbound_port_number < 0 ||
        remote_unbound_port_number >= NUMPORTS)
        return NULL;

    semaphore_P(mutex_bound); // Acquire lock
    for (i = 0; i < NUMPORTS; i++) { // Loop once through entire array
        // The first time through the array, everything is null
        // so this should terminate in O(1) time
        if (bound_ports[next_bound_id] == NULL) { // Found new empty location
            cur_id = next_bound_id;
            increment_bound_id();
            new_bound(cur_id, addr, remote_unbound_port_number);
            semaphore_V(mutex_bound); // Release lock
            return bound_ports[cur_id];
        }
        increment_bound_id();
    }

    semaphore_V(mutex_bound); // Release lock
    return NULL; // All ports are taken
}

/* Destroys a miniport and frees up its resources. If the miniport was in use at
 * the time it was destroyed, subsequent behavior is undefined.
 */
void
miniport_destroy(miniport_t miniport) {
    void * data;
    if ( !miniport ) return;

    if (miniport->port_type == UNBOUND) {
        // Free internal queue and semaphores
        semaphore_P(mutex_unbound); // Acquire lock
        unbound_ports[miniport->port_number] = NULL;
        semaphore_V(mutex_unbound); // Release lock

        while (queue_dequeue(miniport->u.unbound.incoming_data, &data) == 0) {
            free((network_interrupt_arg_t *) data);
        }
        queue_free(miniport->u.unbound.incoming_data);
        semaphore_destroy(miniport->u.unbound.lock);
        semaphore_destroy(miniport->u.unbound.ready);
    } else if (miniport->port_type == BOUND) {
        // Sets to array value to NULL and free
        semaphore_P(mutex_bound); // Acquire lock
        bound_ports[miniport->port_number - NUMPORTS] = NULL;
        semaphore_V(mutex_bound); // Release lock
    }
    free(miniport);
}

/* Sends a message through a locally bound port (the bound port already has an associated
 * receiver address so it is sufficient to just supply the bound port number). In order
 * for the remote system to correctly create a bound port for replies back to the sending
 * system, it needs to know the sender's listening port (specified by local_unbound_port).
 * The msg parameter is a pointer to a data payload that the user wishes to send and does not
 * include a network header; your implementation of minimsg_send must construct the header
 * before calling network_send_pkt(). The return value of this function is the number of
 * data payload bytes sent not inclusive of the header.
 */
int
minimsg_send(miniport_t local_unbound_port, miniport_t local_bound_port, minimsg_t msg, int len) {
    network_address_t my_address;
    mini_header_t header;
    // Null checks
    if ( !local_unbound_port || !local_bound_port || msg == NULL ) return -1;

    // Size check
    if (len > MINIMSG_MAX_MSG_SIZE) return -1;

    network_get_my_address(my_address); // Get my address
    // Construct a header to send
    header = (mini_header_t) malloc (sizeof(struct mini_header));

    if ( !header ) return -1;

    header->protocol = PROTOCOL_MINIDATAGRAM;
    // Pack source and destination
    pack_address(header->source_address, my_address);
    pack_unsigned_short(header->source_port, local_unbound_port->port_number);
    pack_address(header->destination_address, local_bound_port->u.bound.remote_address);
    pack_unsigned_short(header->destination_port, local_bound_port->u.bound.remote_unbound_port);

    // Frees the header after sending the packet
    if (network_send_pkt(local_bound_port->u.bound.remote_address, sizeof(struct mini_header), (char *) header, len, msg) == -1) {
        free(header);
        return -1;
    }
    free(header);
    // Assumes that we have transmitted the whole message if successful
    return len;
}

/* Receives a message through a locally unbound port. Threads that call this function are
 * blocked until a message arrives. Upon arrival of each message, the function must create
 * a new bound port that targets the sender's address and listening port, so that use of
 * this created bound port results in replying directly back to the sender. It is the
 * responsibility of this function to strip off and parse the header before returning the
 * data payload and data length via the respective msg and len parameter. The return value
 * of this function is the number of data payload bytes received not inclusive of the header.
 */
int
minimsg_receive(miniport_t local_unbound_port, miniport_t* new_local_bound_port, minimsg_t msg, int *len) {
    void *node;
    mini_header_t header;
    int payload_size;
    network_interrupt_arg_t *data;
    network_address_t source_address;
    int source_port;

    // Null check
    if ( !local_unbound_port ) {
        *new_local_bound_port = NULL;
        *len = 0;
        return -1;
    }

    // Wait until ready
    semaphore_P(local_unbound_port->u.unbound.ready);

    // Lock incoming data queue to get data
    semaphore_P(local_unbound_port->u.unbound.lock);
    queue_dequeue(local_unbound_port->u.unbound.incoming_data, &node);
    semaphore_V(local_unbound_port->u.unbound.lock);

    data = (network_interrupt_arg_t *) node;

    payload_size = data->size - sizeof(struct mini_header);
    // We have less payload than the allocated buffer
    if (payload_size < *len) {
        *len = payload_size;
    }
    // Copies data over to user buffer
    memcpy(msg, data->buffer + sizeof(struct mini_header), *len);

    // Try to construct bound port
    header = (mini_header_t) data->buffer;
    unpack_address(header->source_address, source_address);
    source_port = unpack_unsigned_short(header->source_port);

    *new_local_bound_port = miniport_create_bound(source_address, source_port);

    // Failed to get a new bound port
    if ( *new_local_bound_port == NULL ) {
        free(data);
        return -1;
    }

    // Successfully created bound port
    free(data);
    return *len;
}

