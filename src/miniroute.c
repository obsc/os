#include "miniroute.h"
#include "synch.h"
#include "cache.h"

cache_t path_cache;
semaphore_t cache_mutex;

/* Performs any initialization of the miniroute layer, if required. */
void miniroute_initialize() {
    path_cache = lru_new(SIZE_OF_ROUTE_CACHE);
    mutex = semaphore_create();

    if ( !path_cache || !mutex ) {
        lru_destroy(path_cache);
        semaphore_destroy(mutex);
    }

    semaphore_initialize(mutex, 1);
}

void flood_discovery() {

}

/* sends a miniroute packet, automatically discovering the path if necessary. See description in the
 * .h file.
 */
int miniroute_send_pkt(network_address_t dest_address, int hdr_len, char* hdr, int data_len, char* data) {
    flood_discovery();
}

/* hashes a network_address_t into a 16 bit unsigned int */
unsigned short hash_address(network_address_t address) {
    unsigned int result = 0;
    int counter;

    for (counter = 0; counter < 3; counter++)
        result ^= ((unsigned short*)address)[counter];

    return result % 65521;
}
