#include <stdlib.h>
#include "counter.h"
#include "synch.h"

struct counter {
    semaphore_t lock;
    semaphore_t sem;
    int count;
};

counter_t counter_new() {
    counter_t counter = (counter_t) malloc (sizeof (struct counter));
    if (!counter) return NULL;

    counter->lock = semaphore_create();
    counter->sem = semaphore_create();
    if (!counter->lock || !counter->sem) {
        semaphore_destroy(counter->lock);
        semaphore_destroy(counter->sem);
        return NULL;
    }

    semaphore_initialize(counter->lock, 1);
    counter->count = 0;

    return counter;
}

void counter_destroy(counter_t counter) {
    semaphore_destroy(counter->lock);
    semaphore_destroy(counter->sem);
    free(counter);
}

void counter_initialize(counter_t counter, int cnt) {
    counter->count = -cnt;
    semaphore_initialize(counter->sem, cnt);
}

void counter_P(counter_t counter) {
    semaphore_P(counter->lock);
    counter->count++;
    semaphore_V(counter->lock);

    semaphore_P(counter->sem);
}

void counter_P_n(counter_t counter, int num) {
    int i;

    semaphore_P(counter->lock);
    counter->count += num;
    semaphore_V(counter->lock);

    for (i = 0; i < num; i++) {
        semaphore_P(counter->sem);
    }
}

void counter_V(counter_t counter) {
    semaphore_P(counter->lock);
    counter->count--;
    semaphore_V(counter->lock);

    semaphore_V(counter->sem);
}

void counter_V_n(counter_t counter, int num) {
    int i;

    semaphore_P(counter->lock);
    counter->count -= num;
    semaphore_V(counter->lock);

    for (i = 0; i < num; i++) {
        semaphore_V(counter->sem);
    }
}

void counter_V_all(counter_t counter) {
    int i;
    int num;

    semaphore_P(counter->lock);
    if (counter->count <= 0) {
        semaphore_V(counter->lock);
        return;
    }
    num = counter->count;
    counter->count = 0;
    semaphore_V(counter->lock);

    for (i = 0; i < num; i++) {
        semaphore_V(counter->sem);
    }
}

int counter_get_count(counter_t counter) {
    return counter->count;
}
