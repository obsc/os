/*
 * TODO: recomment
 * Bounded buffer example. 
 *
 * Sample program that implements a single producer-single consumer
 * system. To be used to test the correctness of the threading and
 * synchronization implementations.
 * 
 * Change MAXCOUNT to vary the number of items produced by the producer.
 */
#include <stdio.h>
#include <stdlib.h>
#include "minithread.h"
#include "synch.h"

#define EMPLOYEES 10
#define CONSUMERS 1000

#define BUFFER 100

int unpack, sell;

semaphore_t empty;
semaphore_t full;
semaphore_t lock;
int consumer(int* arg) {
    semaphore_P(empty);
    sell += 1;
    printf("Phone %i\n", sell);
    semaphore_V(full);

    return 0;
}

int employee(int* arg) {
    while (1) {
        semaphore_P(full);
        semaphore_P(lock);
        unpack += 1;
        semaphore_V(lock);
        //printf("Unpacked phone %i\n", unpack);
        semaphore_V(empty);
        minithread_yield();
    }

    return -1;
}

int spawner(int *arg) {
    int i;
    for (i = 0; i < EMPLOYEES; i++) {
        minithread_fork(employee, NULL);
    }
    for (i = 0; i < CONSUMERS; i++) {
        minithread_fork(consumer, NULL);
    }
    return 0;
}

int
main(int argc, char * argv[]) {
    empty = semaphore_create();
    full = semaphore_create();
    lock = semaphore_create();
    semaphore_initialize(empty, 0);
    semaphore_initialize(full, BUFFER);
    semaphore_initialize(lock, 1);
    unpack = 0;
    sell = 0;

    minithread_system_initialize(spawner, NULL);
    return -1;
}
