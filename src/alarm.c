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
    int time; // Time that the alarm fires at
};

pqueue_t alarm_pqueue;

/* register an alarm to go off in "delay" milliseconds.  Returns a handle to
 * the alarm. Returns NULL on failure.
 */
alarm_id
register_alarm(int delay, alarm_handler_t alarm, void *arg) {
    int t = time_ticks * PERIOD + delay * MILLISECOND; // Time
    alarm_t a = (alarm_t) malloc (sizeof(struct alarm));
    if ( !a ) return NULL; // Failure to malloc

    a->func = alarm;
    a->arg = arg;
    a->time = t;

    if (pqueue_enqueue(alarm_pqueue, a, t) == -1) return NULL;

    return (alarm_id) a;
}

/* unregister an alarm.  Returns 0 if the alarm had not been executed, 1
 * otherwise.
 */
int
deregister_alarm(alarm_id alarm) {
    if (pqueue_delete(alarm_pqueue, alarm) == 0) {
        free(alarm);
        return 0;
    }
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

    while (pqueue_peek(alarm_pqueue, &a) == 0) {
        alarm = (alarm_t) a;
        if (time_ticks * PERIOD >= alarm->time) {
            pqueue_dequeue(alarm_pqueue, &a);
            alarm = (alarm_t) a;
            alarm->func(alarm->arg);
            free(alarm);
        } else {
            break;
        }
    }
}

/*
** vim: ts=4 sw=4 et cindent
*/
