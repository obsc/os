#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void test_new() {
    queue_t q;
    q = queue_new();
    assert(queue_length(q) == 0);
}

void test_prepend() {
    queue_t q;
    int x = 5;
    void* item1 = (void *) &x;
    q = queue_new();
    queue_prepend(q, item1);
    assert(queue_length(q) == 1);
}

void test_append() {

}

void test_dequeue() {

}

void test_length() {

}

void test_iterate() {

}

void test_free() {

}

void test_delete() {

}

int main(void) {
    test_new();
    test_prepend();
    test_append();
    test_dequeue();
    test_length();
    test_iterate();
    test_free();
    test_delete();

    printf("All Tests Pass!!!\n");
    return 0;
}
