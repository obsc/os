/*
 * minithread.c:
 *      This file provides a few function headers for the procedures that
 *      you are required to implement for the minithread assignment.
 *
 *      EXCEPT WHERE NOTED YOUR IMPLEMENTATION MUST CONFORM TO THE
 *      NAMING AND TYPING OF THESE PROCEDURES.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "minithread.h"
#include "queue.h"
#include "synch.h"

#include <assert.h>

/*
 * A minithread should be defined either in this file or in a private
 * header file.  Minithreads have a stack pointer with to make procedure
 * calls, a stackbase which points to the bottom of the procedure
 * call stack, the ability to be enqueueed and dequeued, and any other state
 * that you feel they must have.
 */

struct minithread {
    int id;
    status_t status;
    stack_pointer_t base;
    stack_pointer_t top;
};

stack_pointer_t system_stack;
minithread_t current_thread;
queue_t ready_queue;
queue_t zombie_queue;
int cur_id;

void garbage_collect() {
    void *zomb;
    minithread_t garbage;

    while (queue_length(zombie_queue) > 0) {
        if (queue_dequeue(zombie_queue, &zomb) == 0) {
            garbage = (minithread_t) zomb;
            minithread_free_stack(garbage->base);
            free(garbage);
        }
    }
}

/* Schedules and context switch to the next thread using FCFS, or the idle thread if no threads left */


// TODO: comment this

void
minithread_next() {
    void *next;
    minithread_t old;
    old = current_thread;

    garbage_collect();
    if (old->status == ZOMBIE) {
        queue_append(zombie_queue, old);
    }

    if (queue_length(ready_queue) == 0) {
        minithread_switch(&(old->top), &system_stack);
    } else {
        queue_dequeue(ready_queue, &next);
        current_thread = (minithread_t) next;
        current_thread->status = RUNNING;
        minithread_switch(&(old->top), &(current_thread->top));
    }
}

void
scheduler() {
    void *next;
    while (1) {
        while(queue_length(ready_queue) == 0);

        queue_dequeue(ready_queue, &next);
        current_thread = (minithread_t) next;
        current_thread->status = RUNNING;

        minithread_switch(&system_stack, &(current_thread->top));
        garbage_collect();
    }

}
/* Returns the next available id for minithreads */

int
next_id() {
    return cur_id++;
}

int
minithread_exit(int *i) {
    current_thread->status = ZOMBIE;
    minithread_next();
    return -1;
}

/* minithread functions */

minithread_t
minithread_fork(proc_t proc, arg_t arg) {
    minithread_t t = minithread_create(proc, arg);
    minithread_start(t);

    return t;
}

minithread_t
minithread_create(proc_t proc, arg_t arg) {
    minithread_t t = (minithread_t) malloc (sizeof(struct minithread));
    t->id = next_id();
    t->status = NEW;

    minithread_allocate_stack(&(t->base), &(t->top));
    minithread_initialize_stack(&(t->top), proc, arg, minithread_exit, NULL);

    return t;
}

minithread_t
minithread_self() {
    return current_thread;
}

int
minithread_id() {
    return current_thread->id;
}

/* Does not start ready or zombie queues */
void
minithread_start(minithread_t t) {
    if (t->status != READY && t->status != ZOMBIE) {
        t->status = READY;
        queue_append(ready_queue, t);
    }
}

void
minithread_yield() {
    if (queue_length(ready_queue) > 0) {
        minithread_start(current_thread);
        minithread_next();
    } else {
        garbage_collect();
    }
}

void
minithread_stop() {
    current_thread->status = WAITING;
    minithread_next();
}


/*
 * Initialization.
 *
 *      minithread_system_initialize:
 *       This procedure should be called from your C main procedure
 *       to turn a single threaded UNIX process into a multithreaded
 *       program.
 *
 *       Initialize any private data structures.
 *       Create the idle thread.
 *       Fork the thread which should call mainproc(mainarg)
 *       Start scheduling.
 *
 */
void
minithread_system_initialize(proc_t mainproc, arg_t mainarg) {
    // Initialize globals
    ready_queue = queue_new();
    zombie_queue = queue_new();
    cur_id = 0;
    // Initialize threads
    current_thread = minithread_fork(mainproc, mainarg);
    scheduler();
}

