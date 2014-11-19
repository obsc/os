#include "miniroute.h"
#include "synch.h"
#include "cache.h"

cache_t path_cache;
semaphore_t cache_mutex;

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

    if ( !path_cache || !mutex ) {
        lru_destroy(path_cache);
        semaphore_destroy(mutex);
    }

    semaphore_initialize(mutex, 1);
}

void
flood_discovery() {

}

/* sends a miniroute packet, automatically discovering the path if necessary. See description in the
 * .h file.
 */
int
miniroute_send_pkt(network_address_t dest_address, int hdr_len, char* hdr, int data_len, char* data) {
    int success; // Boolean on wether out cache had the value or not
    char path[MAX_ROUTE_LENGTH][8]; // The path to destination
    int pktlen;

    pktlen = sizeof(routing_header) + hdr_len + data_len;

    // sanity checks
    if (hdr_len < 0 || data_len < 0 || pktlen > MAX_NETWORK_PKT_SIZE)
        return -1;

    semaphore_P(path_cache);
    success = cache_get(path_cache, dest_address, &path);
    semaphore_V(path_cache);

    while (success == -1) {
        flood_discovery();
        semaphore_P(path_cache);
        success = cache_get(path_cache, dest_address, &path);
        semaphore_V(path_cache);
    }
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
