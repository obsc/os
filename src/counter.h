#ifndef __COUNTER_H__
#define __COUNTER_H__

#include "synch.h"

/*
 *  A semaphore that can release multiple at the same time
 */

typedef struct counter* counter_t;

extern counter_t counter_new();

extern void counter_destroy(counter_t);

extern void counter_initialize(counter_t, int);

extern void counter_P(counter_t, int safe);

extern void counter_P_n(counter_t, int, int safe);

extern void counter_V(counter_t, int safe);

extern void counter_V_n(counter_t, int, int safe);

extern void counter_V_all(counter_t, int safe);

extern int counter_get_count(counter_t);

#endif /*__COUNTER_H__*/
