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
#include "minimsg.h"
#include "minisocket.h"
#include "miniheader.h"
#include "synch.h"
#include "alarm.h"
#include "queue.h"
#include "multilevel_queue.h"
#include "miniroute.h"
#include "read_private.h"
#include "disk.h"
#include "minifile.h"

#include <assert.h>
#include <time.h>

#define LEVELS 4
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
    int level; // The priority level of the thread
    stack_pointer_t base;
    stack_pointer_t top;
};

stack_pointer_t system_stack; // Stack pointer to the system thread
minithread_t cur_thread; // Thread control block of the current thread
multilevel_queue_t ready_queue; // Queue for ready threads

queue_t zombie_queue; // Queue for zombie threads for cleanup
semaphore_t garbage; // Semaphore representing garbage needed to be collected

int cur_id; // Current id (used to assign new ids)
int quanta_passed; // The amount of quanta that has passed for current thread
long time_ticks; // Current time in number of interrupt ticks

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
            set_interrupt_level(old_level);
            garbage_thread = (minithread_t) zomb;
            minithread_free_stack(garbage_thread->base); // Frees the stack
            free(garbage_thread); // Frees the thread control block
        }
    }
    return -1;
}

/* Function decides the level of the multilevel queue to start trying to
 * dequeue from, then dequeues starting from that level.
 * Returns -1 if nothing dequeued, or the level from which it dequeued from
 * invariant: this function should be called with interrupts disabled
 */
int next_item(void **location) {
    int rng;
    int level;
    rng = rand() % 100;
    // 50% for level 0, 25% for level 1, 15% for level 2, 10% for level 3
    if (rng < 50) {
        level = 0;
    } else if (rng < 75) {
        level = 1;
    } else if (rng < 90) {
        level = 2;
    } else {
        level = 3;
    }
    return multilevel_queue_dequeue(ready_queue, level, location);
}

/*
 * Function to get the next thread and switch to it
 * It is assumed that interrupts are already disabled when calling this function
 * Additionally, it is also assumed that the ready_queue is not empty
 */
void switch_next(stack_pointer_t *stack) {
    void *next;

    next_item(&next);
    cur_thread = (minithread_t) next;
    cur_thread->status = RUNNING;
    minithread_switch(stack, &(cur_thread->top));
}

/*
 * Minithread scheduler that decides the next thread
 * (either the system or another minithread) to be run
 * This swaps over to the system if there are no more ready threads.
 * invariant: when calling minithread_next(), interrupts are already
 * disabled, interrupts enabled when exiting
 */
void minithread_next() {
    minithread_t old;
    old = cur_thread;
    // if we are looking for the next thread,
    // then we set next thread's used quanta to 0
    quanta_passed = 0;

    // if there are no more runnable threads, return to the system
    if (multilevel_queue_length(ready_queue) == 0) {
        cur_thread = NULL;
        minithread_switch(&(old->top), &system_stack);
    // if there are runnable threads, take the next one and run it
    } else {
        switch_next(&(old->top));
    }
}

/* Called after a thread ends operation */
int minithread_exit(int *i) {
    interrupt_level_t old_level;

    old_level = set_interrupt_level(DISABLED);
    cur_thread->status = ZOMBIE;
    queue_append(zombie_queue, cur_thread);
    semaphore_V(garbage);
    minithread_next();
    return -1;
}

/* minithread functions */

/*
 * Creates a new thread and sets it to runnable
 */
minithread_t minithread_fork(proc_t proc, arg_t arg) {
    minithread_t t = minithread_create(proc, arg);
    minithread_start(t);

    return t;
}

/*
 * Creates a new thread control block. Returns NULL on failure
 */
minithread_t minithread_create(proc_t proc, arg_t arg) {
    interrupt_level_t old_level;
    minithread_t t = (minithread_t) malloc (sizeof(struct minithread));
    if ( !t ) return NULL;

    old_level = set_interrupt_level(DISABLED);
    t->id = cur_id++; // Disables interrupt to access cur_id
    set_interrupt_level(old_level);

    t->status = NEW;
    t->level = 0;

    minithread_allocate_stack(&(t->base), &(t->top));
    minithread_initialize_stack(&(t->top), proc, arg, minithread_exit, NULL);

    return t;
}

/*
 * Gets the thread control block of the currently running thread
 */
minithread_t minithread_self() {
    return cur_thread;
}

/*
 * Gets the id of the currently running thread
 */
int minithread_id() {
    return cur_thread->id;
}

/*
 * Makes a minithread runnable
 * Does not work on already READY or ZOMBIE threads
 */
void minithread_start(minithread_t t) {
    interrupt_level_t old_level;

    if (t->status != READY && t->status != ZOMBIE) {
        t->status = READY;
        t->level = 0;
        old_level = set_interrupt_level(DISABLED);
        multilevel_queue_enqueue(ready_queue, t->level, t);
        set_interrupt_level(old_level);
    }
}

/*
 * Yields from the current thread, allowing others to run
 */
void minithread_yield() {
    interrupt_level_t old_level = set_interrupt_level(DISABLED);

    // Only reenqueue if there are other threads waiting to be run
    // Otherwise, just return
    if (multilevel_queue_length(ready_queue) > 0) {
        minithread_start(cur_thread);
        minithread_next();
    }

    // We set this to maintain the invariant that yielding sets your quanta to 0
    quanta_passed = 0;
    set_interrupt_level(old_level);
}

/*
 * Stops the current thread and does not set to runnable again.
 */
void minithread_stop() {
    interrupt_level_t old_level = set_interrupt_level(DISABLED);
    cur_thread->status = WAITING;
    minithread_next();
    set_interrupt_level(old_level);
}

/*
 * Unblocks a semaphore (used by sleep)
 */
void unblock(void *sem) {
    semaphore_V((semaphore_t) sem);
}

/*
 * sleep with timeout in milliseconds
 */
void minithread_sleep_with_timeout(int delay) {
    interrupt_level_t old_level;
    semaphore_t sleep_done;

    old_level = set_interrupt_level(DISABLED);

    sleep_done = semaphore_create();
    semaphore_initialize(sleep_done, 0);
    // set alarm to wake up thread after delay
    register_alarm(delay, unblock, sleep_done);
    // wait until alarm wakes up thread
    semaphore_P(sleep_done);
    semaphore_destroy(sleep_done);

    set_interrupt_level(old_level);
}

/*
 * This is the clock interrupt handling routine.
 * You have to call minithread_clock_init with this
 * function as parameter in minithread_system_initialize
 */
void clock_handler(void* arg) {
    interrupt_level_t old_level = set_interrupt_level(DISABLED);

    time_ticks++;
    check_alarms();
    // only deal with quanta logic when not in system thread
    if (cur_thread != NULL) {
        quanta_passed++;
        // if the thread used up all its quanta, demote its priority
        if (quanta_passed == (1 << cur_thread->level)) {
            cur_thread->status = READY;
            if (cur_thread->level < LEVELS - 1) {
                cur_thread->level++;
            }
            multilevel_queue_enqueue(ready_queue,cur_thread->level,cur_thread);
            // only context switch if current thread used up its quanta
            minithread_next();
        }
    } else if (multilevel_queue_length(ready_queue) > 0) {
        // if we are idling and a new thread is on the ready queue
        switch_next(&system_stack);
    }
    set_interrupt_level(old_level);
}

/*
 * Interrupt handler to handle receiving packets
 * Does nothing if either source or destination ports are invalid port numbers
 * Does nothing if there is no unbound port instantiated at destination
 */
void
network_handler(network_interrupt_arg_t *arg) {
    interrupt_level_t old_level;

    if ( !arg ) return;

    old_level = set_interrupt_level(DISABLED);
    miniroute_handle(arg);
    set_interrupt_level(old_level);
}

void disk_handler(void *arg) {
    disk_interrupt_arg_t *interrupt;
    interrupt = (disk_interrupt_arg_t *) arg;
    //TODO
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
void minithread_system_initialize(proc_t mainproc, arg_t mainarg) {
    interrupt_level_t old_level;
    disk = (disk_t *) malloc (sizeof(struct disk_t));
    //initialize disk
    disk_initialize(disk);
    install_disk_handler(disk_handler);
    // Use time to seed the random function
    srand(time(NULL));
    // Initialize globals
    ready_queue = multilevel_queue_new(LEVELS);
    zombie_queue = queue_new();
    cur_id = 0;
    quanta_passed = 0;
    // Initialize alarms
    time_ticks = 0;
    initialize_alarms();
    // Intialize garbage collection
    garbage = semaphore_create();
    semaphore_initialize(garbage, 0);
    // Initialize threads
    minithread_fork(reaper, NULL);
    cur_thread = minithread_create(mainproc, mainarg);
    // Disable interrupts for first switch
    old_level = set_interrupt_level(DISABLED);
    // Initialize clock
    minithread_clock_init(PERIOD * MILLISECOND, clock_handler);
    // Initialize network
    network_initialize(network_handler);
    miniroute_initialize();
    minimsg_initialize();
    miniterm_initialize();
    minisocket_initialize();
    // Switch into our first thread
    minithread_switch(&system_stack, &(cur_thread->top));
    // Idles
    while (1);
}
