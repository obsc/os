#include "multilevel_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void test_new() {
    multilevel_queue_t q;
    q = multilevel_queue_new(4);
    assert(multilevel_queue_length(q) == 0);
    assert(multilevel_queue_free(q) == 0);
}

void test_enqueue() {
    multilevel_queue_t q;
    int x1 = 5;
    int x2 = 6;
    int x3 = 7;
    void *value = NULL;
    // Testing null queue
    assert(multilevel_queue_enqueue(NULL, 0, &x1) == -1);
    // Testing queue
    q = multilevel_queue_new(4);
    assert(multilevel_queue_enqueue(q, 6, &x2) == -1);

    assert(multilevel_queue_enqueue(q, 2, &x1) == 0);
    assert(multilevel_queue_length(q) == 1);
    assert(multilevel_queue_enqueue(q, 1, &x2) == 0);
    assert(multilevel_queue_length(q) == 2);
    assert(multilevel_queue_enqueue(q, 3, &x3) == 0);
    assert(multilevel_queue_length(q) == 3);
    assert(multilevel_queue_dequeue(q, 2, (&value)) == 2);
    assert(*((int*) value) == x1);
    assert(multilevel_queue_dequeue(q, 3, (&value)) == 3);
    assert(*((int*) value) == x3);
    assert(multilevel_queue_dequeue(q, 1, (&value)) == 1);
    assert(*((int*) value) == x2);
    assert(multilevel_queue_length(q) == 0);
    assert(multilevel_queue_free(q) == 0);
}

void test_dequeue() {
    multilevel_queue_t q;
    int x1 = 5;
    int x2 = 6;
    int x3 = 7;
    void *value;
    // Testing null queue
    assert(multilevel_queue_dequeue(NULL, &value) == -1);
    assert(value == NULL);
    // Testing empty queue
    q = multilevel_queue_new(2);
    assert(multilevel_queue_dequeue(q, &value) == -1);
    assert(value == NULL);
    // Testing queue
    assert(multilevel_queue_enqueue(q, 0, &x2) == 0);
    assert(multilevel_queue_enqueue(q, 1, &x3) == 0);
    assert(multilevel_queue_enqueue(q, 0, &x1) == 0);
    assert(multilevel_queue_dequeue(q, 0, (&value)) == 0);
    assert(*((int*) value) == x2);
    assert(multilevel_queue_length(q) == 2);
    assert(multilevel_queue_dequeue(q, 1, (&value)) == 1);
    assert(*((int*) value) == x3);
    assert(multilevel_queue_length(q) == 1);
    assert(multilevel_queue_dequeue(q, 1, &value) == 0);
    assert(*((int*) value) == x2);
    assert(multilevel_queue_length(q) == 0);
    assert(multilevel_queue_dequeue(q, 0, &value) == -1);
    assert(multilevel_queue_free(q) == 0);
}

/*
 * This is tested via valgrind
 */
void test_free() {
    multilevel_queue_t q1, q2, q3;
    int i;
    int i1 = 0;
    int i2 = 0;
    // Test null queue
    assert(multilevel_queue_free(NULL) == -1);
    // Test empty queue
    q1 = multilevel_queue_new(2);
    assert(multilevel_queue_free(q1) == 0);
    // Test queue with elements
    q2 = multilevel_queue_new(2);
    assert(multilevel_queue_enqueue(q2, 0, &i1) == 0);
    assert(multilevel_queue_enqueue(q2, 1, &i2) == 0);
    assert(multilevel_queue_free(q2) == 0);
    // Stress test for leaks
    q3 = multilevel_queue_new(2);
    for (i = 0; i < 10; i++) {
        assert(multilevel_queue_enqueue(q3, 0, &i1) == 0);
        assert(multilevel_queue_enqueue(q3, 1, &i2) == 0);
    }
    assert(multilevel_queue_free(q3) == 0);
}

void test_length() {
    multilevel_queue_t q1;
    int i1 = 0;
    int i2 = 0;
    void *p = NULL;
    // Test null queue
    assert(multilevel_queue_length(NULL) == -1);
    // Test queues
    q1 = multilevel_queue_new(2);
    assert(multilevel_queue_length(q1) == 0);
    assert(multilevel_queue_enqueue(q1, 1, &i1) == 0);
    assert(multilevel_queue_length(q1) == 1);
    assert(multilevel_queue_enqueue(q1, 1, &i2) == 0);
    assert(multilevel_queue_length(q1) == 2);
    assert(multilevel_queue_enqueue(q1, 0, &i1) == 0);
    assert(multilevel_queue_length(q1) == 3);
    assert(multilevel_queue_enqueue(q1, 0, &i2) == 0);
    assert(multilevel_queue_length(q1) == 4);
    assert(multilevel_queue_dequeue(q1, 0, &p) == 0);
    assert(multilevel_queue_length(q1) == 3);
    assert(multilevel_queue_dequeue(q1, 0, &p) == 0);
    assert(multilevel_queue_length(q1) == 3);
    assert(multilevel_queue_dequeue(q1, 1, &p) == 1);
    assert(multilevel_queue_dequeue(q1, 1, &p) == 1);
    assert(multilevel_queue_length(q1) == 1);
    assert(multilevel_queue_free(q1) == 0);
}



int main(void) {
    test_new();
    test_enqueue();
    test_dequeue();
    test_free();
    test_length();

    printf("All Tests Pass!!!\n");
    return 0;
}
