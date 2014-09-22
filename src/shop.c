/*
 * This program implements a retail store in which there are employees
 * and customers. The employees act as producers and constantly unbox
 * new phones, while the customers act as consumers and buy phones.
 *
 * Change the macros EMPLOYEES and CONSUMERS to modify the number of each.
 */
#include <stdio.h>
#include <stdlib.h>
#include "minithread.h"
#include "synch.h"

#define EMPLOYEES 10
#define CUSTOMERS 1000

int sold_id; // We will assume that ids are 1 indexed
//int unpack_id;
semaphore_t phones;

/*
 * The consumer thread. They take a single phone and then print its id.
 */
int customer(int* arg) {
    semaphore_P(phones);
    sold_id += 1;
    printf("Bought phone %i\n", sold_id);

    return 0;
}

/*
 * The producer thread. They constantly unbox new phones.
 */
int employee(int* arg) {
    while (1) {
        //unpack_id += 1;
        //printf("Unpacked phone %i\n", unpack_id);
        semaphore_V(phones);
        minithread_yield();
    }

    return -1; // This thread should never end
}

/*
 * A starting thread to spawn all of the employees and customers.
 */
int spawner(int *arg) {
    int i;
    for (i = 0; i < EMPLOYEES; i++) {
        minithread_fork(employee, NULL);
    }
    for (i = 0; i < CUSTOMERS; i++) {
        minithread_fork(customer, NULL);
    }
    return 0;
}

int
main(int argc, char * argv[]) {
    phones = semaphore_create();
    semaphore_initialize(phones, 0);
    sold_id = 0;
    //unpack_id = 0;

    minithread_system_initialize(spawner, NULL);
    return -1;
}
