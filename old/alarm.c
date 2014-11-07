#include <stdlib.h>
#include <stdio.h>

#include "interrupts.h"
#include "alarm.h"
#include "minithread.h"
#include "pqueue.h"

typedef struct alarm* alarm_t;

struct alarm
{
    alarm_handler_t func; // Function to be called when alarm fires
    void *arg; // Argument into the function
    long time; // Time that the alarm fires at
};

pqueue_t alarm_pqueue;

/* register an alarm to go off in "delay" milliseconds.  Returns a handle to
 * the alarm. Returns NULL on failure.
 */
alarm_id
register_alarm(int delay, alarm_handler_t alarm, void *arg) {
    interrupt_level_t old_level;
    long t = time_ticks * PERIOD + delay; // Time
    alarm_t a = (alarm_t) malloc (sizeof(struct alarm));
    if ( !a ) return NULL; // Failure to malloc

    a->func = alarm;
    a->arg = arg;
    a->time = t;
    old_level = set_interrupt_level(DISABLED);
    // Attempt to enqueue onto priority queue
    if (pqueue_enqueue(alarm_pqueue, a, t) == -1) return NULL;
    set_interrupt_level(old_level);
    return (alarm_id) a;
}

/* unregister an alarm.  Returns 0 if the alarm had not been executed, 1
 * otherwise.
 */
int
deregister_alarm(alarm_id alarm) {
    interrupt_level_t old_level;
    old_level = set_interrupt_level(DISABLED);
    if (pqueue_delete(alarm_pqueue, alarm) == 0) {
        free(alarm); // Only frees if alarm is found
        set_interrupt_level(old_level);
        return 0;
    }
    set_interrupt_level(old_level);
    return 1;
}

/*
 * Initialize alarm priority queue
 */
void initialize_alarms() {
    alarm_pqueue = pqueue_new();
}

/*
 * Checks all available alarms and runs their handlers
 */
void check_alarms() {
    alarm_id a;
    alarm_t alarm;

    // Keep checking queue until we find an alarm that should not execute
    while (pqueue_peek(alarm_pqueue, &a) == 0) {
        alarm = (alarm_t) a;
        if (time_ticks * PERIOD >= alarm->time) {
            // Executes the alarm
            pqueue_dequeue(alarm_pqueue, &a);
            alarm = (alarm_t) a;
            alarm->func(alarm->arg);
            free(alarm); // Frees alarm after execution
        } else {
            break;
        }
    }
}

/*
** vim: ts=4 sw=4 et cindent
*/
