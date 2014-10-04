#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void test_new() {
    pqueue_t q;
    q = pqueue_new();
    assert(pqueue_length(q) == 0);
    assert(pqueue_free(q) == 0);
}

void test_enqueue() {
    pqueue_t q;
    int x1 = 5;
    int x2 = 6;
    int x3 = 7;
    void *value = NULL;
    // Testing null queue
    assert(pqueue_enqueue(NULL, &x1, 0) == -1);
    // Testing queue
    q = pqueue_new();
    assert(pqueue_enqueue(q, &x1, 2) == 0);
    assert(pqueue_length(q) == 1);
    assert(pqueue_enqueue(q, &x2, 1) == 0);
    assert(pqueue_length(q) == 2);
    assert(pqueue_enqueue(q, &x3, 3) == 0);
    assert(pqueue_length(q) == 3);
    assert(pqueue_dequeue(q, (&value)) == 0);
    assert(*((int*) value) == x2);
    assert(pqueue_dequeue(q, (&value)) == 0);
    assert(*((int*) value) == x1);
    assert(pqueue_dequeue(q, (&value)) == 0);
    assert(*((int*) value) == x3);
    assert(pqueue_length(q) == 0);
    assert(pqueue_free(q) == 0);
}

void test_dequeue() {
    pqueue_t q;
    int x1 = 5;
    int x2 = 6;
    int x3 = 7;
    void *value;
    // Testing null queue
    assert(pqueue_dequeue(NULL, &value) == -1);
    assert(value == NULL);
    // Testing empty queue
    q = pqueue_new();
    assert(pqueue_dequeue(q, &value) == -1);
    assert(value == NULL);
    // Testing queue
    assert(pqueue_enqueue(q, &x2, 3) == 0);
    assert(pqueue_enqueue(q, &x3, 2) == 0);
    assert(pqueue_enqueue(q, &x1, 1) == 0);
    assert(pqueue_dequeue(q, (&value)) == 0);
    assert(*((int*) value) == x1);
    assert(pqueue_length(q) == 2);
    assert(pqueue_dequeue(q, (&value)) == 0);
    assert(*((int*) value) == x3);
    assert(pqueue_length(q) == 1);
    assert(pqueue_dequeue(q, &value) == 0);
    assert(*((int*) value) == x2);
    assert(pqueue_length(q) == 0);
    assert(pqueue_dequeue(q, &value) == -1);
    assert(pqueue_free(q) == 0);
}

void test_peek() {
    pqueue_t q;
    int x1 = 5;
    int x2 = 6;
    int x3 = 7;
    void *value;
    // Testing null queue
    assert(pqueue_peek(NULL, &value) == -1);
    assert(value == NULL);
    // Testing empty queue
    q = pqueue_new();
    assert(pqueue_peek(q, &value) == -1);
    assert(value == NULL);
    // Testing queue
    assert(pqueue_enqueue(q, &x2, 3) == 0);
    assert(pqueue_enqueue(q, &x3, 2) == 0);
    assert(pqueue_enqueue(q, &x1, 1) == 0);
    assert(pqueue_peek(q, (&value)) == 0);
    assert(*((int*) value) == x1);
    assert(pqueue_length(q) == 3);
    assert(pqueue_peek(q, (&value)) == 0);
    assert(*((int*) value) == x3);
    assert(pqueue_length(q) == 3);
    assert(pqueue_peek(q, &value) == 0);
    assert(*((int*) value) == x2);
    assert(pqueue_length(q) == 3);
    assert(pqueue_peek(q, &value) == -1);
    assert(pqueue_free(q) == 0);
}

/*
 * This is tested via valgrind
 */
void test_free() {
    pqueue_t q1, q2, q3;
    int i;
    int i1 = 0;
    int i2 = 0;
    // Test null queue
    assert(pqueue_free(NULL) == -1);
    // Test empty queue
    q1 = pqueue_new();
    assert(pqueue_free(q1) == 0);
    // Test queue with elements
    q2 = pqueue_new();
    assert(pqueue_enqueue(q2, &i1, 0) == 0);
    assert(pqueue_enqueue(q2, &i2, 0) == 0);
    assert(pqueue_free(q2) == 0);
    // Stress test for leaks
    q3 = pqueue_new();
    for (i = 0; i < 10; i++) {
        assert(pqueue_enqueue(q3, &i1, 0) == 0);
        assert(pqueue_enqueue(q3, &i2, 1) == 0);
    }
    assert(pqueue_free(q3) == 0);
}

void test_length() {
    pqueue_t q1;
    int i1 = 0;
    int i2 = 0;
    void *p = NULL;
    // Test null queue
    assert(pqueue_length(NULL) == -1);
    // Test queues
    q1 = pqueue_new();
    assert(pqueue_length(q1) == 0);
    assert(pqueue_enqueue(q1, &i1, 1) == 0);
    assert(pqueue_length(q1) == 1);
    assert(pqueue_enqueue(q1, &i2, 1) == 0);
    assert(pqueue_length(q1) == 2);
    assert(pqueue_enqueue(q1, &i1, 1) == 0);
    assert(pqueue_length(q1) == 3);
    assert(pqueue_enqueue(q1, &i2, 1) == 0);
    assert(pqueue_length(q1) == 4);
    assert(pqueue_dequeue(q1, &p) == 0);
    assert(pqueue_length(q1) == 3);
    assert(pqueue_delete(q1, NULL) == -1);
    assert(pqueue_length(q1) == 3);
    assert(pqueue_delete(q1, &i1) == 0);
    assert(pqueue_delete(q1, &i2) == 0);
    assert(pqueue_length(q1) == 1);
    assert(pqueue_free(q1) == 0);
}

void test_delete() {
    pqueue_t q1, q2, q3, q4;
    int i;
    int i1 = 0;
    int i2 = 0;
    int i3 = 0;
    // Test null queue
    assert(pqueue_delete(NULL, NULL) == -1);
    assert(pqueue_delete(NULL, &i1) == -1);
    // Test deleting from empty queue
    q1 = pqueue_new();
    assert(pqueue_delete(q1, NULL) == -1);
    assert(pqueue_delete(q1, &i1) == -1);
    assert(pqueue_free(q1) == 0);
    // Test deleting from first element of queue
    q2 = pqueue_new();
    assert(pqueue_enqueue(q2, &i2, 1) == 0);
    assert(pqueue_enqueue(q2, &i2, 1) == 0);
    assert(pqueue_delete(q2, NULL) == -1);
    assert(pqueue_delete(q2, &i1) == -1);
    assert(pqueue_delete(q2, &i2) == 0);
    assert(pqueue_delete(q2, &i2) == 0);
    assert(pqueue_delete(q2, &i2) == -1);
    assert(pqueue_free(q2) == 0);
    // Test deleting from last element of queue
    q3 = pqueue_new();
    assert(pqueue_enqueue(q3, &i2, 1) == 0);
    assert(pqueue_enqueue(q3, &i3, 2) == 0);
    assert(pqueue_enqueue(q3, &i3, 3) == 0);
    assert(pqueue_delete(q3, &i1) == -1);
    assert(pqueue_delete(q3, &i3) == 0);
    assert(pqueue_delete(q3, &i3) == 0);
    assert(pqueue_delete(q3, &i3) == -1);
    assert(pqueue_free(q3) == 0);
    // Test deleting from any part of queue
    q4 = pqueue_new();
    for (i = 0; i < 3; i++) {
        assert(pqueue_enqueue(q4, &i1, 1) == 0);
        assert(pqueue_enqueue(q4, &i2, 3) == 0);
    }
    assert(pqueue_enqueue(q4, &i3, 3) == 0);
    assert(pqueue_enqueue(q4, &i1, 2) == 0);
    assert(pqueue_enqueue(q4, &i2, 1) == 0);
    assert(pqueue_delete(q4, &i3) == 0);
    assert(pqueue_delete(q4, &i3) == -1);
    for (i = 0; i < 4; i++) {
        assert(pqueue_delete(q4, &i1) == 0);
        assert(pqueue_delete(q4, &i2) == 0);
    }
    assert(pqueue_delete(q4, &i1) == -1);
    assert(pqueue_delete(q4, &i2) == -1);
    assert(pqueue_free(q4) == 0);
}

int main(void) {
    test_new();
    test_enqueue();
    test_dequeue();
    test_peek();
    test_free();
    test_length();
    test_delete();

    printf("All Tests Pass!!!\n");
    return 0;
}
