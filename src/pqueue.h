/*
 * Priority queue manipulation functions
 */
#ifndef __PQUEUE_H__
#define __PQUEUE_H__

#include "alarm.h"

/*
 * pqueue_t is a pointer to an internally maintained data structure.
 */
typedef struct pqueue* pqueue_t;

/*
 * Return an empty priority queue.  Returns NULL on error.
 */
extern pqueue_t pqueue_new();

/*
 * Enqueues a void* to a priority queue (both specifed as parameters).
 * Returns 0 (success) or -1 (failure).
 */
extern int pqueue_enqueue(pqueue_t, alarm_handler_t, void*, int priority);

/*
 * Dequeue and return the first node from the priority queue.
 * Return 0 (success) and first item if pqueue is nonempty, or -1 (failure) and
 * NULL if pqueue is empty.
 */
extern int pqueue_dequeue(pqueue_t, alarm_id *);

/*
 * Return the first node from the priority queue without dequeueing.
 * Return 0 (success) and first item if queue is pnonempty, or -1 (failure) and
 * NULL if pqueue is empty.
 */
extern int pqueue_peek(pqueue_t, alarm_id *);

/*
 * Free the priority queue and return 0 (success) or -1 (failure).
 */
extern int pqueue_free (pqueue_t);

/*
 * Return the number of items in the priority queue, or -1 if an error occured
 */
extern int pqueue_length(pqueue_t queue);

/*
 * Delete the first instance of the specified item from the given queue.
 * Returns 0 if an element was deleted, or -1 otherwise.
 */
extern int pqueue_delete(pqueue_t queue, alarm_id node);

#endif /*__PQUEUE_H__*/
