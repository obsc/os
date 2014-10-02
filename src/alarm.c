#include <stdlib.h>
#include <stdio.h>

#include "interrupts.h"
#include "alarm.h"
#include "minithread.h"
#include "pqueue.h"

typedef struct alarm* alarm_t;

struct alarm
{
    alarm_handler_t func;
    void *arg;
}

pqueue_t alarm_pqueue;

/* see alarm.h */
alarm_id
register_alarm(int delay, alarm_handler_t alarm, void *arg) {
    int t = delay; // Time
    alarm_t a = (alarm_t) malloc (sizeof(struct alarm));
    a->func = alarm;
    a->arg = arg;

    pqueue_enqueue(alarm_pqueue, a, t);

    return (alarm_id) a;
}

/* see alarm.h */
int
deregister_alarm(alarm_id alarm) {
    return 1;
}

/*
 * Initialize alarm priority queue
 */
void initialize_alarms() {
    alarm_pqueue = pqueue_new();
}

/*
** vim: ts=4 sw=4 et cindent
*/
