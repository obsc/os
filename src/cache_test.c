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

void test_set_get() {
    cache_t c;
    int data;
    int data2;
    int data3;
    void *evicted;
    void *output;
    network_address_t addr;
    network_address_t addr2;
    network_get_my_address(addr);
    network_get_my_address(addr2);
    data = 5;
    data2 = 6;
    data3 = 7;
    addr2[1] = 2;
    addr[1] = 1;

    // Testing null cache
    assert(cache_set(NULL, addr, &data, &evicted) == -1);
    assert(cache_get(NULL, addr, &output) == -1);
    assert(evicted == NULL);
    assert(output == NULL);
    // Testing setting and getting
    c = cache_new(1);
    assert(cache_get(c,addr,&output) == -1);
    assert(cache_set(c, addr, &data, &evicted) == 0);
    assert(evicted == NULL);
    assert(cache_get(c,addr,&output) == 0);
    assert(*((int *) output) == 5);
    // Testing replacing
    assert(cache_set(c, addr, &data2, &evicted) == 0);
    assert(evicted == NULL);
    assert(cache_get(c,addr,&output) == 0);
    assert(*((int *) output) == 6);
    // Testing evicting
    assert(cache_set(c, addr2, &data3, &evicted) == 0);
    assert(evicted == &data2);
    assert(cache_get(c,addr,&output) == -1);
    assert(cache_get(c,addr2,&output) == 0);
    assert(*((int *) output) == 7);
    assert(cache_destroy(c) == 0);
    // Testing coexistance
    c = cache_new(2);
    assert(cache_set(c, addr, &data2, &evicted) == 0);
    assert(evicted == NULL);
    assert(cache_get(c,addr,&output) == 0);
    assert(*((int *) output) == 6);
    assert(cache_set(c, addr2, &data3, &evicted) == 0);
    assert(cache_get(c,addr,&output) == 0);
    assert(*((int *) output) == 6);
    assert(cache_get(c,addr2,&output) == 0);
    assert(*((int *) output) == 7);
    assert(cache_destroy(c) == 0);

}

void test_delete_destroy() {
    cache_t c;
    network_address_t addr;
    network_address_t addr2;
    int data;
    int data2;
    void *evicted;
    void *output;
    network_get_my_address(addr);
    network_get_my_address(addr2);
    addr2[1] = 2;
    addr[1] = 1;
    data = 5;
    data2 = 6;
    // Testing null cache
    assert(cache_delete(NULL, addr) == -1);
    assert(cache_destroy(NULL) == -1);
    // Testing delete
    c = cache_new(2);
    assert(cache_set(c, addr, &data, &evicted) == 0);
    assert(cache_set(c, addr2, &data2, &evicted) == 0);
    assert(cache_delete(c, addr) == 0);
    assert(cache_delete(c, addr) == -1);
    assert(cache_delete(c, addr2) == 0);
    assert(cache_get(c,addr,&output) == -1);
    assert(cache_get(c,addr2,&output) == -1);
    // Testing destroy
    assert(cache_destroy(c) == 0);
    c = NULL;
    assert(cache_set(c, addr, &data, &evicted) == -1);
    assert(cache_set(c, addr2, &data2, &evicted) == -1);

}

int main(void) {
    test_new();
    test_set_get();
    test_delete_destroy();

    printf("All Tests Pass!!!\n");
    return 0;
}
