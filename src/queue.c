/*
 * Generic queue implementation.
 *
 */
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

struct node
{
    void   *data;
    node_t next;
};

struct queue
{
    node_t head;
    node_t tail;
};

/*
 * Return an empty queue.
 */
queue_t
queue_new() {
    queue_t q = (queue_t) malloc (sizeof(struct queue));
    q->head = NULL;
    q->tail = NULL;
    return q;
}

/*
 * Prepend a void* to a queue (both specifed as parameters).  Return
 * 0 (success) or -1 (failure).
 */
int
queue_prepend(queue_t queue, void* item) {
    node_t n = (node_t) malloc (sizeof(struct node));
    if (n == NULL) {
        return -1;
    }

    n->data = item;
    n->next = queue->head;
    if ( !(queue->head) ) {
        queue->tail = n;
    }

    queue->head = n;
    return 0;
}

/*
 * Append a void* to a queue (both specifed as parameters). Return
 * 0 (success) or -1 (failure).
 */
int
queue_append(queue_t queue, void* item) {
    node_t n = (node_t) malloc (sizeof(struct node));
    if (n == NULL) {
        return -1;
    }

    n->data = item;
    n->next = NULL;

    if (queue->head) {
        queue->tail->next = n;
    } else {
        queue->head = n;
    }

    queue->tail = n;
    return 0;
}

/*
 * Dequeue and return the first void* from the queue or NULL if queue
 * is empty.  Return 0 (success) or -1 (failure).
 */
int
queue_dequeue(queue_t queue, void** item) {
    node_t n = queue->head;
    queue->head = n->next;

    if ( !(queue->head) ) {
        queue->tail = NULL;
    }

    *item = n->data;
    free(n);

    if ( n == NULL ) {
        return -1;
    }
    return 0;
}

/*
 * Iterate the function parameter over each element in the queue.  The
 * additional void* argument is passed to the function as its first
 * argument and the queue element is the second.  Return 0 (success)
 * or -1 (failure).
 */
int
queue_iterate(queue_t queue, func_t f, void* item) {
    return 0;
}

/*
 * Free the queue and return 0 (success) or -1 (failure).
 */
int
queue_free (queue_t queue) {
    return 0;
}

/*
 * Return the number of items in the queue.
 */
int
queue_length(queue_t queue) {
    return 0;
}


/*
 * Delete the specified item from the given queue.
 * Return -1 on error.
 */
int
queue_delete(queue_t queue, void* item) {
    return 0;
}
