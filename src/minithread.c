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

minithread_t current_thread;
queue_t ready_queue;
queue_t zombie_queue;
int cur_id;

/* Returns the next available id for minithreads */

int next_id() {
    return cur_id++;
}

/* minithread functions */

minithread_t
minithread_fork(proc_t proc, arg_t arg) {
    return (minithread_t)0;
}

minithread_t
minithread_create(proc_t proc, arg_t arg) {
    return (minithread_t)0;
}

minithread_t
minithread_self() {
    return current_thread;
}

int
minithread_id() {
    return current_thread->id;
}

void
minithread_stop() {
}

void
minithread_start(minithread_t t) {
}

void
minithread_yield() {
}

void
minithread_exit() {
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
}


