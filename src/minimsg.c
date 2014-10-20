/*
 *  Implementation of minimsgs and miniports.
 */
#include <stdlib.h>
#include "minimsg.h"
#include "queue.h"
#include "synch.h"
#include "network.h"

struct miniport {
    char port_type;
    int port_number;

    union {
        struct {
            queue_t incoming_data;
            semaphore_t lock; // TODO: Maybe remove this?
            semaphore_t ready;
        } unbound;
        struct {
            network_address_t remote_address;
            int remote_unbound_port;
        } bound;
    } u;
};

miniport_t unbound_ports[NUMPORTS]; // Array of all the unbound ports
miniport_t bound_ports[NUMPORTS]; // Array of all the bound ports
int next_bound_id; // Next bound port id to use (NUMPORTS less than port number)

void
network_handler(network_interrupt_arg_t *arg) {

}

/* performs any required initialization of the minimsg layer.
 */
void
minimsg_initialize() {
    int i;
    // Initialize network with handler
    network_initialize(network_handler);
    // Initialize global id counter
    next_bound_id = 0;
    // Initializes both tables to 0
    for (i = 0; i < NUMPORTS; i++) {
        unbound_ports[i] = NULL;
        bound_ports[i] = NULL;
    }
}

/*
 * Instantiates a new unbound port and puts it into the array of ports
 * Should only be called when the port is NULL
 */
void
new_unbound(int port_number) {
    miniport_t port = (miniport_t) malloc (sizeof(struct miniport));
    port->port_type = UNBOUND;
    port->port_number = port_number;
    port->u.unbound.incoming_data = queue_new();
    port->u.unbound.lock = semaphore_create(); // TODO: Maybe remove?
    port->u.unbound.ready = semaphore_create();

    unbound_ports[port_number] = port;
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

    if (unbound_ports[port_number] == NULL) {
        new_unbound(port_number);
    }
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
    // Out of range check
    if (remote_unbound_port_number < NUMPORTS ||
        remote_unbound_port_number >= 2 * NUMPORTS) return NULL;

    return 0;
}

/* Destroys a miniport and frees up its resources. If the miniport was in use at
 * the time it was destroyed, subsequent behavior is undefined.
 */
void
miniport_destroy(miniport_t miniport) {
    if (miniport == NULL) return;

    if (miniport->port_type == UNBOUND) {
        // Free internal queue and semaphores
        queue_free(miniport->u.unbound.incoming_data);
        semaphore_destroy(miniport->u.unbound.lock); // TODO: Maybe remove?
        semaphore_destroy(miniport->u.unbound.ready);
        // Sets to array value to NULL and free
        unbound_ports[miniport->port_number] = NULL;
        free(miniport);
    } else if (miniport->port_type == BOUND) {

    }
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
    return 0;
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
    return 0;
}

