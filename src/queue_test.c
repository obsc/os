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

void test_length() {

}

void test_iterate() {

}

void test_free() {
    // Test null queue
    assert(queue_free(NULL) == -1);
}

void test_delete() {
    // Declarations
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
    test_length();
    test_iterate();
    test_free();
    test_delete();

    printf("All Tests Pass!!!\n");
    return 0;
}
