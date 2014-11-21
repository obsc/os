#include "cache.h"
#include "network.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void test_new() {
    cache_t c;
    c = cache_new(9);
    assert(cache_destroy(c) == 0);
}

void test_set() {
    cache_t c;
    int *data;
    void *evicted;
    void *output;
    network_address_t addr;
    network_address_t addr2;
    network_get_my_address(addr);
    network_get_my_address(addr2);
    *data = 5;
    addr2[1] = 2;

    // Testing null cache
    assert(cache_set(NULL, addr, (void *)data, &evicted) == -1);
    assert(evicted == NULL);
    // Testing setting
    c = cache_new(1);
    assert(cache_set(c, addr, (void *)data, &evicted) == 0);
    assert(evicted == NULL);
    assert(cache_get(c,addr,&output) == 0);
    assert(*((int *) output) == 5);
    // Testing replacing
    *data = 6;
    assert(cache_set(c, addr, (void *)data, &evicted) == 0);
    assert(evcited == NULL);
    assert(cache_get(c,addr,&output) == 0);
    assert(*((int *) output) == 6);
    // Testing evicting
    *data = 7
    assert(cache_set(c, addr2, (void *)data, &evicted) == 0);
    assert(*((int *) evicted) == 6);
    assert(cache_get(c,addr,&output) == -1);
    assert(cache_get(c,addr2,&output) == 0);
    assert(*((int *) output) == 7);





}

int main(void) {
    test_new();
    test_set();

    printf("All Tests Pass!!!\n");
    return 0;
}
