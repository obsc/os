#include "defs.h"
#include "network.h"
#include "miniroute.h"
#include "miniheader.h"
#include "synch.h"
#include "cache.h"

cache_t path_cache;
semaphore_t cache_mutex;

char *dummy; // Represents a dummy bytearray to pass to send pkt

int
bcast_empty(char* hdr) {
    return network_bcast_pkt(sizeof(struct routing_header), hdr, 0, dummy);
}

int
send(network_address_t dest_address, char* hdr, int len, char* data) {
    return network_send_pkt(dest_address, sizeof(struct routing_header), hdr, len, data);
}

int
send_empty(network_address_t dest_address, char* hdr) {
    return send(dest_address, hdr, 0, dummy);
}

/* Handler for miniroutes messages
 * Assumes interrupts are disabled within
 */
void
miniroute_handle(network_interrupt_arg_t *arg) {
    // Check protocol type
    if (arg->buffer[0] == PROTOCOL_MINIDATAGRAM) {
        minimsg_handle(arg);
    } else if (arg->buffer[0] == PROTOCOL_MINISTREAM) {
        minisocket_handle(arg);
    }
}

/* Performs any initialization of the miniroute layer, if required. */
void
miniroute_initialize() {
    path_cache = lru_new(SIZE_OF_ROUTE_CACHE);
    mutex = semaphore_create();
    dummy = (char *) malloc(1);

    if ( !path_cache || !mutex || !dummy ) {
        lru_destroy(path_cache);
        semaphore_destroy(mutex);
        free(dummy);
        return;
    }

    semaphore_initialize(mutex, 1);
}

/*
 * Creates a new discovery header to a destination
 * Returns null on failure
 */
routing_header_t
create_disc_hdr(network_address_t dest_address, int id) {
    network_address_t my_address;
    routing_header_t header;

    header = (routing_header_t) malloc (sizeof(struct routing_header));
    if (!header) return NULL;

    network_get_my_address(my_address); // Get my address

    header->routing_packet_type = ROUTING_ROUTE_DISCOVERY;
    pack_address(header->destination, dest_address);
    pack_unsigned_int(header->id, id); // TODO: CHANGE THIS
    pack_unsigned_int(header->ttl, MAX_ROUTE_LENGTH);
    pack_unsigned_int(header->path_len, 1);
    pack_address(header->path[0], my_address);

    return header;
}

/*
 * Creates a reply header by using the discovery header
 */
routing_header_t
create_reply_hdr(routing_header_t hdr) {
    int pathlen;
    int i;
    char temp[8];

    pathlen = unpack_unsigned_int(header->path_len);

    header->routing_packet_type = ROUTING_ROUTE_REPLY;
    memcpy(header->destination, hdr->path[0], 8); // Original source of packet
    pack_unsigned_int(header->ttl, MAX_ROUTE_LENGTH);

    for (i = 0; i < pathlen / 2; i++) { // Reverses the path
        memcpy(temp, header->path[i], 8);
        memcpy(header->path[i], header->path[pathlen - 1 - i], 8);
        memcpy(header->path[pathlen - 1 - i], temp, 8);
    }

    return header;
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

/* Performs flood broadcasting to discover path to destination
 * Returns -1 on failure and 0 on success
 */
int
flood_discovery(network_address_t dest_address) {
    routing_header_t header;

    header = create_disc_hdr(dest_address);
    if (!header) return -1;

    bcast_empty((char *) header);
    free(header);

    return 0;
}

/* sends a miniroute packet, automatically discovering the path if necessary. See description in the
 * .h file.
 */
int
miniroute_send_pkt(network_address_t dest_address, int hdr_len, char* hdr, int data_len, char* data) {
    int pathlen; // Length of the path
    int pktlen; // Size of the packet
    char path[MAX_ROUTE_LENGTH][8]; // The path to destination
    routing_header_t data_hdr;
    char* full_data;

    pktlen = sizeof(routing_header) + hdr_len + data_len;

    // sanity checks
    if (hdr_len < 0 || data_len < 0 || pktlen > MAX_NETWORK_PKT_SIZE)
        return -1;

    // Checks if the path is in the cache
    semaphore_P(path_cache);
    pathlen = cache_get(path_cache, dest_address, &path);
    semaphore_V(path_cache);

    // If not, discovery protocol, then check again
    while (pathlen == -1) { // TODO: CHANGE THIS WHEN PIAZZA QUESTION RESOLVED
        flood_discovery();
        semaphore_P(path_cache);
        pathlen = cache_get(path_cache, dest_address, &path);
        semaphore_V(path_cache);
    }
    // We have successfully gotten a path
    data_hdr = create_data_hdr(dest_address, pathlen, &path);

    full_data = (char *) malloc(hdr_len + data_len);
    memcpy(full_data, hdr, hdr_len);
    memcpy(full_data + hdr_len, data, data_len);

    if (send(dest_address, data_hdr, hdr_len + data_len, full_data) == -1) {
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
