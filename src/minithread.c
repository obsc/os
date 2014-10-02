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
#include "interrupts.h"
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
    int id; // Id of the minithread
    status_t status; // Status: NEW, WAITING, READY, RUNNING, ZOMBIE
    stack_pointer_t base;
    stack_pointer_t top;
};

stack_pointer_t system_stack; // Stack pointer to the system thread
minithread_t current_thread; // Thread control block of the current thread
queue_t ready_queue; // Queue for ready threads
queue_t zombie_queue; // Queue for zombie threads for cleanup
int cur_id; // Current id (used to assign new ids)
semaphore_t garbage; // Semaphore representing garbage needed to be collected

/*
 * Thread that garbage collects all the garbage in the zombie queue.
 * This should only be ran when there is garbage, otherwise it will block.
 */
int reaper(int *arg) {
    void *zomb;
    minithread_t garbage_thread;
    interrupt_level_t old_level;

    while (1) {
        semaphore_P(garbage);
        old_level = set_interrupt_level(DISABLED);
        if (queue_dequeue(zombie_queue, &zomb) == 0) {
            garbage_thread = (minithread_t) zomb;
            minithread_free_stack(garbage_thread->base); // Frees the stack
            free(garbage_thread); // Frees the thread control block
        }
        set_interrupt_level(old_level);
    }
    return -1;
}

/*
 * Minithread scheduler that decides the next thread
 * (either the system or another minithread) to be run
 * This swaps over to the system if there are no more ready threads.
 * invariant: when calling minithread_next(), interrupts are already
 * disabled, interrupts enabled when exiting
 */
void
minithread_next() {
    void *next;
    minithread_t old;
    old = current_thread;

    // if there are no more runnable threads, return to the system
    if (queue_length(ready_queue) == 0) {
        current_thread = NULL;
        minithread_switch(&(old->top), &system_stack);
    // if there are runnable threads, take the next one and run it
    } else {
        queue_dequeue(ready_queue, &next);
        current_thread = (minithread_t) next;
        current_thread->status = RUNNING;
        minithread_switch(&(old->top), &(current_thread->top));
    }
}

/*
 * The system scheduler that decides the next thread to run
 * or to idle. This idles whenever there are no more threads in
 * the ready queue
 */
void
scheduler() {
    void *next;
    interrupt_level_t old_level;

    while (1) {
        // idle if there are no runnable threads
        while(queue_length(ready_queue) == 0);

        old_level = set_interrupt_level(DISABLED);
        queue_dequeue(ready_queue, &next);
        current_thread = (minithread_t) next;
        current_thread->status = RUNNING;

        minithread_switch(&system_stack, &(current_thread->top));
    }
}

/* Called after a thread ends operation */
int
minithread_exit(int *i) {
    interrupt_level_t old_level;

    old_level = set_interrupt_level(DISABLED);
    current_thread->status = ZOMBIE;
    queue_append(zombie_queue, current_thread);
    semaphore_V(garbage);
    minithread_next();
    return -1;
}

/* minithread functions */

/*
 * Creates a new thread and sets it to runnable
 */
minithread_t
minithread_fork(proc_t proc, arg_t arg) {
    minithread_t t = minithread_create(proc, arg);
    minithread_start(t);

    return t;
}

/*
 * Creates a new thread control block
 */
minithread_t
minithread_create(proc_t proc, arg_t arg) {
    interrupt_level_t old_level = set_interrupt_level(DISABLED);

    minithread_t t = (minithread_t) malloc (sizeof(struct minithread));
    t->id = cur_id++;
    t->status = NEW;

    minithread_allocate_stack(&(t->base), &(t->top));
    minithread_initialize_stack(&(t->top), proc, arg, minithread_exit, NULL);

    set_interrupt_level(old_level);
    return t;
}

/*
 * Gets the thread control block of the currently running thread
 */
minithread_t
minithread_self() {
    return current_thread;
}

/*
 * Gets the id of the currently running thread
 */
int
minithread_id() {
    return current_thread->id;
}

/*
 * Makes a minithread runnable
 * Does not work on already READY or ZOMBIE threads
 */
void
minithread_start(minithread_t t) {
    interrupt_level_t old_level = set_interrupt_level(DISABLED);

    if (t->status != READY && t->status != ZOMBIE) {
        t->status = READY;
        queue_append(ready_queue, t);
    }
    set_interrupt_level(old_level);
}

/*
 * Yields from the current thread, allowing others to run
 * If this is the only runnable thread, instead of rescheduling,
 * we just NOP and garbage collect any previously exited threads
 * instead.
 */
void
minithread_yield() {
    interrupt_level_t old_level = set_interrupt_level(DISABLED);

    if (queue_length(ready_queue) > 0) {
        minithread_start(current_thread);
        minithread_next();
    }
    set_interrupt_level(old_level);
}

/*
 * Stops the current thread and does not set to runnable again.
 */
void
minithread_stop() {
    interrupt_level_t old_level = set_interrupt_level(DISABLED);

    current_thread->status = WAITING;
    minithread_next();
    set_interrupt_level(old_level);
}


/*
 * This is the clock interrupt handling routine.
 * You have to call minithread_clock_init with this
 * function as parameter in minithread_system_initialize
 */
void
clock_handler(void* arg) {
    interrupt_level_t old_level = set_interrupt_level(DISABLED);
    if (current_thread != NULL) {
        minithread_yield();
    }
    set_interrupt_level(old_level);
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
    interrupt_level_t old_level;
    // Initialize globals
    ready_queue = queue_new();
    zombie_queue = queue_new();
    cur_id = 0;

    garbage = semaphore_create();
    semaphore_initialize(garbage, 0);
    current_thread = NULL;
    // Initialize threads
    minithread_fork(reaper, NULL);
    minithread_fork(mainproc, mainarg);
    minithread_clock_init(100 * MILLISECOND, clock_handler);
    // Disable interrupts
    old_level = set_interrupt_level(DISABLED);
    scheduler();
}

/*
 * sleep with timeout in milliseconds
 */
void
minithread_sleep_with_timeout(int delay) {

}

