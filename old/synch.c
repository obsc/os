#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "interrupts.h"
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
    queue_t waiting; // Waiting queue for the semaphore
    int count;
};


/*
 * semaphore_t semaphore_create()
 *      Allocate a new semaphore. Return NULL on failure
 */
semaphore_t semaphore_create() {
    semaphore_t sem = (semaphore_t) malloc (sizeof (struct semaphore));
    if ( !sem ) return NULL;

    sem->waiting = queue_new();
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
    interrupt_level_t old_level;

    old_level = set_interrupt_level(DISABLED);

    if (--sem->count < 0) { // No more resources; block until V
        queue_append(sem->waiting, minithread_self());
        minithread_stop();
    }

    set_interrupt_level(old_level);
}

/*
 * semaphore_V(semaphore_t sem)
 *      V on the sempahore.
 */
void semaphore_V(semaphore_t sem) {
    void *next;
    minithread_t next_thread;
    interrupt_level_t old_level;

    old_level = set_interrupt_level(DISABLED);

    if (++sem->count <= 0) { // Unblocks one element in the queue
        queue_dequeue(sem->waiting, &next);
        next_thread = (minithread_t) next;
        minithread_start(next_thread);
    }

    set_interrupt_level(old_level);
}
