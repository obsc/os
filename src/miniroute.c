#include "defs.h"
#include "network.h"
#include "miniroute.h"
#include "miniheader.h"
#include "minimsg.h"
#include "minisocket.h"
#include "synch.h"
#include "cache.h"
#include "alarm.h"

#define NUM_RETRY 3
#define WAIT_DELAY 12000

typedef struct route {
    int path_len;
    char path[MAX_ROUTE_LENGTH][8];
}* route_t;

typedef struct waiting {
    int num_waiting;
    int id;
    semaphore_t wait_disc;
    semaphore_t wait_for_data;
    route_t route;    
}* waiting_t;

cache_t wait_cache;
cache_t path_cache;
semaphore_t path_mutex;
semaphore_t wait_mutex;
semaphore_t wait_limit; // Limits the number of concurrent discoveries

char *dummy; // Represents a dummy bytearray to pass to send pkt
unsigned int next_id; // Represents the currnt id value to send for discoveries

/* Increments and returns the next id
 */
unsigned int
get_next_id() {
    return next_id++;
}

/* Creates a new waiting block
 */
waiting_t
create_waiting() {
    waiting_t wait;

    wait = (waiting_t) malloc(sizeof(struct waiting));
    if (!wait) return NULL;

    wait->num_waiting = 1;
    wait->wait_disc = semaphore_create();
    wait->wait_for_data = semaphore_create();
    wait->route = NULL;

    if ( !wait->wait_disc || !wait->wait_for_data ) {
        semaphore_destroy(wait->wait_disc);
        semaphore_destroy(wait->wait_for_data);
        free(wait);
        return NULL;
    }

    wait->id = get_next_id();
    semaphore_initialize(wait->wait_disc, 0);
    semaphore_initialize(wait->wait_for_data, 0);
    return wait;
}

/* Destroys a waiting block
 */
void
destroy_waiting(waiting_t wait) {
    semaphore_destroy(wait->wait_disc);
    semaphore_destroy(wait->wait_for_data);
    free(wait);
}

/* Sends out a broadcast for discovery
 * Appends current address to the path as well
 */
int
bcast_discovery(routing_header_t hdr) {
    network_address_t my_address;
    int pathlen;

    if (unpack_unsigned_int(hdr->ttl) == 0) { // Fail if ttl is 0
        return 0;
    }

    network_get_my_address(my_address);
    pathlen = unpack_unsigned_int(hdr->path_len);
    pack_unsigned_int(hdr->path_len, pathlen + 1);
    pack_address(hdr->path[pathlen], my_address);

    return network_bcast_pkt(sizeof(struct routing_header), (char *) hdr, 0, dummy);
}

/* Sends a data packet along a path
 *
 */
int
send_data(routing_header_t hdr, int len, char* data) {
    network_address_t next_addr; // Next address along path
    int i; // Index of the current node in the path

    i = MAX_ROUTE_LENGTH - unpack_unsigned_int(hdr->ttl);
    if (i >= MAX_ROUTE_LENGTH - 1) {
        return 0;
    }

    unpack_address(hdr->path[i+1], next_addr);
    return network_send_pkt(next_addr, sizeof(struct routing_header), (char *) hdr, len, data);
}

/* Sends a reply along a path
 *
 */
int
send_reply(routing_header_t hdr) {
    return send_data(hdr, 0, dummy);
}

/* Time passes for the header (decrease ttl)
 *
 */
void
increment_hdr(routing_header_t hdr) {
    pack_unsigned_int(hdr->ttl, unpack_unsigned_int(hdr->ttl) - 1);
}

/*
 * Creates a new discovery header to a destination
 * Returns null on failure
 */
routing_header_t
create_disc_hdr(network_address_t dest_address, int id) {
    routing_header_t header;

    header = (routing_header_t) malloc (sizeof(struct routing_header));
    if (!header) return NULL;

    header->routing_packet_type = ROUTING_ROUTE_DISCOVERY;
    pack_address(header->destination, dest_address);
    pack_unsigned_int(header->id, id);
    pack_unsigned_int(header->ttl, MAX_ROUTE_LENGTH);
    pack_unsigned_int(header->path_len, 0);

    return header;
}

/*
 * Creates a reply header by using the discovery header
 */
routing_header_t
create_reply_hdr(routing_header_t hdr) {
    network_address_t my_address;
    int pathlen;
    int i;
    char temp[8];

    pathlen = unpack_unsigned_int(hdr->path_len);
    network_get_my_address(my_address);

    hdr->routing_packet_type = ROUTING_ROUTE_REPLY;
    memcpy(hdr->destination, hdr->path[0], 8); // Original source of packet
    pack_unsigned_int(hdr->ttl, MAX_ROUTE_LENGTH);
    pack_unsigned_int(hdr->path_len, pathlen + 1);
    pack_address(hdr->path[pathlen], my_address);

    for (i = 0; i < (pathlen + 1) / 2; i++) { // Reverses the path
        memcpy(temp, hdr->path[i], 8);
        memcpy(hdr->path[i], hdr->path[pathlen - 1 - i], 8);
        memcpy(hdr->path[pathlen - 1 - i], temp, 8);
    }

    return hdr;
}

/*
 * Creates a new data header to a destination with a path
 * Returns null on failure
 */
routing_header_t
create_data_hdr(network_address_t dest_address, int len, char (*path)[8]) {
    network_address_t my_address;
    routing_header_t header;
    int i;

    header = (routing_header_t) malloc (sizeof(struct routing_header));
    if (!header) return NULL;

    network_get_my_address(my_address); // Get my address

    header->routing_packet_type = ROUTING_DATA;
    pack_address(header->destination, dest_address);
    pack_unsigned_int(header->id, 0);
    pack_unsigned_int(header->ttl, MAX_ROUTE_LENGTH);
    pack_unsigned_int(header->path_len, len);

    for (i = 0; i < len; i++) {
        memcpy(header->path[i], path[i], 8);
    }

    return header;
}

/* Checks if we are the destination of the packet
 * Return 0 if not, non-zero if destination
 */
int
check_destination(routing_header_t hdr) {
    network_address_t my_address;
    network_address_t dest_address;

    network_get_my_address(my_address);
    unpack_address(hdr->destination, dest_address);
    return network_compare_network_addresses(my_address, dest_address);
}

/* Checks if we are in the path of the packet
 * Return 0 if not, non-zero if we are
 */
int
check_route(routing_header_t hdr) {
    int pathlen;
    int i;
    network_address_t my_address;
    network_address_t other_address;

    network_get_my_address(my_address);
    pathlen = unpack_unsigned_int(hdr->path_len);

    for (i = 0; i < pathlen; i++) {
        unpack_address(hdr->path[i], other_address);
        if (network_compare_network_addresses(my_address, other_address) > 0) {
            return 1;
        }
    }
    return 0;
}

/* Handler for miniroutes messages
 * Assumes interrupts are disabled within
 */
void
miniroute_handle(network_interrupt_arg_t *arg) {
    routing_header_t header;
    routing_header_t reply_hdr;

    header = (routing_header_t) arg->buffer;

    // Check routing type
    if (header->routing_packet_type == ROUTING_ROUTE_DISCOVERY) {
        if (check_destination(header) > 0) { // We have reached the destination
            reply_hdr = create_reply_hdr(header);
            send_reply(reply_hdr);
            free(reply_hdr);
        } else { // Rebroadcast
            increment_hdr(header);
            if (!check_route(header)) {
                bcast_discovery(header);
            }
        }
        free(arg);
    } else if (header->routing_packet_type == ROUTING_ROUTE_REPLY) {
        if (check_destination(header) > 0) { // We have reached the destination

        } else { // Continue sending
            increment_hdr(header);
            send_reply(header);
        }
        free(arg);
    } else if (header->routing_packet_type == ROUTING_DATA) {
        if (check_destination(header) > 0) { // We have reached the destination
            // Check protocol type
            if (arg->buffer[sizeof(struct routing_header)] == PROTOCOL_MINIDATAGRAM) {
                minimsg_handle(arg);
            } else if (arg->buffer[sizeof(struct routing_header)] == PROTOCOL_MINISTREAM) {
                minisocket_handle(arg);
            }
        } else { // Continue sending
            increment_hdr(header);
            send_data(header, arg->size - sizeof(struct routing_header), arg->buffer + sizeof(struct routing_header));
            free(arg);
        }
    }
}

/* Performs any initialization of the miniroute layer, if required. */
void
miniroute_initialize() {
    wait_cache = cache_new(SIZE_OF_ROUTE_CACHE);
    path_cache = cache_new(SIZE_OF_ROUTE_CACHE);
    wait_mutex = semaphore_create();
    path_mutex = semaphore_create();
    wait_limit = semaphore_create();
    dummy = (char *) malloc(1);

    if ( !wait_cache || !path_cache || !wait_mutex ||
         !path_mutex || !wait_limit || !dummy ) {
        cache_destroy(wait_cache);
        cache_destroy(path_cache);
        semaphore_destroy(wait_mutex);
        semaphore_destroy(path_mutex);
        semaphore_destroy(wait_limit);
        free(dummy);
        return;
    }

    semaphore_initialize(wait_mutex, 1);
    semaphore_initialize(path_mutex, 1);
    semaphore_initialize(wait_limit, SIZE_OF_ROUTE_CACHE);
}

void
fail_wait(waiting_t wait) {
    int i;

    for (i = 0; i < wait->num_waiting - 1; i++) {
        semaphore_V(wait->wait_for_data);
    }
}

/*
 * Times out a waiting flood discovery
 */
void timeout(void *w) {
    waiting_t wait;

    wait = (waiting_t) w;
    semaphore_V(wait->wait_disc);
}

/* Performs flood broadcasting to discover path to destination
 */
void
flood_discovery(network_address_t dest_address, waiting_t wait) {
    int i;
    routing_header_t header;
    alarm_id retry_alarm;

    header = create_disc_hdr(dest_address, wait->id);
    if (!header) {
        fail_wait(wait);
        return;
    }

    for (i = 0; i < NUM_RETRY; i++) {
        retry_alarm = register_alarm(WAIT_DELAY, timeout, wait);
        bcast_discovery(header);
        semaphore_P(wait->wait_disc);
        if (wait->route != NULL) { // Success
            free(header);
            return;
        }
    }

    // 3 failures
    free(header);
    fail_wait(wait);
}

/* sends a miniroute packet, automatically discovering the path if necessary. See description in the
 * .h file.
 */
int
miniroute_send_pkt(network_address_t dest_address, int hdr_len, char* hdr, int data_len, char* data) {
    int cache_hit; // If we hit the path cache or not
    int pktlen; // Size of the packet
    routing_header_t data_hdr;
    char* full_data;
    void* route_node;
    void* wait_node;
    void* overflow_node;
    route_t route;
    waiting_t wait;

    pktlen = sizeof(struct routing_header) + hdr_len + data_len;

    // sanity checks
    if (hdr_len < 0 || data_len < 0 || pktlen > MAX_NETWORK_PKT_SIZE)
        return -1;

    // Checks if the path is in the cache
    semaphore_P(path_mutex);
    cache_hit = cache_get(path_cache, dest_address, &route_node);
    semaphore_V(path_mutex);
    route = (route_t) route_node;

    if (cache_hit == -1) { // Cache miss
        semaphore_P(wait_mutex);
        if (cache_get(wait_cache, dest_address, &wait_node) == -1) { // New dest
            semaphore_V(wait_mutex);
            semaphore_P(wait_limit);
            wait = create_waiting();
            cache_set(wait_cache, dest_address, wait, &overflow_node); // should not overflow
            flood_discovery(dest_address, wait);
        } else {
            semaphore_V(wait_mutex);
            wait = (waiting_t) wait_node;
            wait->num_waiting++;
            semaphore_P(wait->wait_for_data);
        }
        if (wait->route == NULL) { // Route discovery failure
            wait->num_waiting--;
            if (wait->num_waiting == 0) { // If last out, remove
                cache_delete(wait_cache, dest_address);
                destroy_waiting(wait);
                semaphore_V(wait_limit);
            }
            return -1;
        } else { // Route discovery success
            route = wait->route;
        }
    }

    // We have successfully gotten a path
    data_hdr = create_data_hdr(dest_address, route->path_len, route->path);

    full_data = (char *) malloc(hdr_len + data_len);
    memcpy(full_data, hdr, hdr_len);
    memcpy(full_data + hdr_len, data, data_len);

    if (send_data(data_hdr, hdr_len + data_len, full_data) == -1) {
        free(data_hdr);
        return -1;
    }

    free(data_hdr);
    return hdr_len + data_len;
}

/* hashes a network_address_t into a 16 bit unsigned int */
unsigned short
hash_address(network_address_t address) {
    unsigned int result = 0;
    int counter;

    for (counter = 0; counter < 3; counter++)
        result ^= ((unsigned short*)address)[counter];

    return result % 65521;
}
