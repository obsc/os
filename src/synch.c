#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "defs.h"
#include "synch.h"
#include "queue.h"
#include "minithread.h"
#include "machineprimitives.h"

/*
 *      You must implement the procedures and types defined in this interface.
 */


/*
 * Semaphores.
 */
struct semaphore {
    queue_t waiting;
    tas_lock_t lock;
    int count;
};


/*
 * semaphore_t semaphore_create()
 *      Allocate a new semaphore.
 */
semaphore_t semaphore_create() {
    semaphore_t sem = (semaphore_t) malloc (sizeof (struct semaphore));
    sem->waiting = queue_new();
    sem->lock = 0;
    sem->count = 0;
    return sem;
}

/*
 * semaphore_destroy(semaphore_t sem);
 *      Deallocate a semaphore.
 */
void semaphore_destroy(semaphore_t sem) {
    queue_free(sem->waiting);
    free(sem);
}


/*
 * semaphore_initialize(semaphore_t sem, int cnt)
 *      initialize the semaphore data structure pointed at by
 *      sem with an initial value cnt.
 */
void semaphore_initialize(semaphore_t sem, int cnt) {
    sem->count = cnt;
}

/*
 * semaphore_P(semaphore_t sem)
 *      P on the sempahore.
 */
void semaphore_P(semaphore_t sem) {
    while (atomic_test_and_set(&sem->lock) == 1)
        minithread_yield();

    if (--sem->count < 0) {
        queue_append(sem->waiting, minithread_self());
        atomic_clear(&sem->lock);
        minithread_stop();
    } else {
        atomic_clear(&sem->lock);
    }
}

/*
 * semaphore_V(semaphore_t sem)
 *      V on the sempahore.
 */
void semaphore_V(semaphore_t sem) {
    void *next;
    minithread_t next_thread;
    while (atomic_test_and_set(&sem->lock) == 1)
        minithread_yield();

    if (++sem->count <= 0) {
        queue_dequeue(sem->waiting, &next);
        next_thread = (minithread_t) next;
        minithread_start(next_thread);
    }

    atomic_clear(&sem->lock);
}
