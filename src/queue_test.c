#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void test_new() {

}

void test_prepend() {

}

void test_append() {

}

void test_dequeue() {

}

void length_helper(void *x, void *a) {
    *(int *)a += 1;
}

void sum_helper(void *x, void *a) {
    *(int *)a += *(int *)x;
}

void test_iterate() {
    queue_t q1;
    int i0 = 0;
    int i1 = 1;
    int i2 = 2;
    int i3 = 5;
    // Testing null queue
    assert(queue_iterate(NULL, length_helper, &i0) == -1);
    q1 = queue_new();
    // Testing null function
    assert(queue_iterate(q1, NULL, &i0) == -1);
    // Empty queue
    i0 = 0;
    assert(queue_iterate(q1, length_helper, &i0) == 0);
    assert(i0 == 0);
    i0 = 0;
    assert(queue_iterate(q1, sum_helper, &i0) == 0);
    assert(i0 == 0);
    // Filled queue
    assert(queue_append(q1, &i1) == 0);
    assert(queue_append(q1, &i2) == 0);
    assert(queue_append(q1, &i3) == 0);
    i0 = 0;
    assert(queue_iterate(q1, length_helper, &i0) == 0);
    assert(i0 = 3);
    i0 = 0;
    assert(queue_iterate(q1, sum_helper, &i0) == 0);
    assert(i0 == 8);
    assert(queue_free(q1) == 0);
}

/*
 * This is tested via valgrind
 */
void test_free() {
    queue_t q1, q2, q3;
    int i;
    int i1 = 0;
    int i2 = 0;
    // Test null queue
    assert(queue_free(NULL) == -1);
    // Test empty queue
    q1 = queue_new();
    assert(queue_free(q1) == 0);
    // Test queue with elements
    q2 = queue_new();
    assert(queue_append(q2, &i1) == 0);
    assert(queue_append(q2, &i2) == 0);
    assert(queue_free(q2) == 0);
    // Stress test for leaks
    q3 = queue_new();
    for (i = 0; i < 10; i++) {
        assert(queue_append(q3, &i1) == 0);
        assert(queue_append(q3, &i2) == 0);
    }
    assert(queue_free(q3) == 0);
}

void test_length() {
    queue_t q1;
    int i1 = 0;
    int i2 = 0;
    void *p = NULL;
    // Test null queue
    assert(queue_length(NULL) == -1);
    // Test queues
    q1 = queue_new();
    assert(queue_length(q1) == 0);
    assert(queue_append(q1, &i1) == 0);
    assert(queue_length(q1) == 1);
    assert(queue_prepend(q1, &i2) == 0);
    assert(queue_length(q1) == 2);
    assert(queue_prepend(q1, &i1) == 0);
    assert(queue_length(q1) == 3);
    assert(queue_append(q1, &i2) == 0);
    assert(queue_length(q1) == 4);
    assert(queue_dequeue(q1, &p) == 0);
    assert(queue_length(q1) == 3);
    assert(queue_delete(q1, NULL) == -1);
    assert(queue_length(q1) == 3);
    assert(queue_delete(q1, &i1) == 0);
    assert(queue_delete(q1, &i2) == 0);
    assert(queue_length(q1) == 1);
    assert(queue_free(q1) == 0);
}

void test_delete() {
    queue_t q1, q2, q3, q4;
    int i;
    int i1 = 0;
    int i2 = 0;
    int i3 = 0;
    // Test null queue
    assert(queue_delete(NULL, NULL) == -1);
    assert(queue_delete(NULL, &i1) == -1);
    // Test deleting from empty queue
    q1 = queue_new();
    assert(queue_delete(q1, NULL) == -1);
    assert(queue_delete(q1, &i1) == -1);
    assert(queue_free(q1) == 0);
    // Test deleting from first element of queue
    q2 = queue_new();
    assert(queue_append(q2, &i2) == 0);
    assert(queue_append(q2, &i2) == 0);
    assert(queue_delete(q2, NULL) == -1);
    assert(queue_delete(q2, &i1) == -1);
    assert(queue_delete(q2, &i2) == 0);
    assert(queue_delete(q2, &i2) == 0);
    assert(queue_delete(q2, &i2) == -1);
    assert(queue_free(q2) == 0);
    // Test deleting from last element of queue
    q3 = queue_new();
    assert(queue_append(q3, &i2) == 0);
    assert(queue_append(q3, &i3) == 0);
    assert(queue_append(q3, &i3) == 0);
    assert(queue_delete(q3, &i1) == -1);
    assert(queue_delete(q3, &i3) == 0);
    assert(queue_delete(q3, &i3) == 0);
    assert(queue_delete(q3, &i3) == -1);
    assert(queue_free(q3) == 0);
    // Test deleting from any part of queue
    q4 = queue_new();
    for (i = 0; i < 3; i++) {
        assert(queue_append(q4, &i1) == 0);
        assert(queue_append(q4, &i2) == 0);
    }
    assert(queue_append(q4, &i3) == 0);
    assert(queue_append(q4, &i1) == 0);
    assert(queue_append(q4, &i2) == 0);
    assert(queue_delete(q4, &i3) == 0);
    assert(queue_delete(q4, &i3) == -1);
    for (i = 0; i < 4; i++) {
        assert(queue_delete(q4, &i1) == 0);
        assert(queue_delete(q4, &i2) == 0);
    }
    assert(queue_delete(q4, &i1) == -1);
    assert(queue_delete(q4, &i2) == -1);
    assert(queue_free(q4) == 0);
}

int main(void) {
    test_new();
    test_prepend();
    test_append();
    test_dequeue();
    test_iterate();
    test_free();
    test_length();
    test_delete();

    printf("All Tests Pass!!!\n");
    return 0;
}
