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
    node_t n = NULL;
    if ( !(queue) ) {
        return -1;
    }

    n = (node_t) malloc (sizeof(struct node));
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
    node_t n = NULL;
    if ( !(queue) ) {
        return -1;
    }

    n = (node_t) malloc (sizeof(struct node));
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
    node_t n = NULL;
    if ( !(queue) ) {
        return -1;
    }

    n = queue->head;
    if ( n == NULL ) {
        *item = NULL;
        return -1;
    }

    queue->head = n->next;

    if ( !(queue->head) ) {
        queue->tail = NULL;
    }

    *item = n->data;
    free(n);

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
    node_t n = NULL;
    if ( !(queue) || !(f) ) {
        return -1;
    }

    n = queue->head;
    while (n) {
        f(n->data, item);

        n = n->next;
    }

    return 0;
}

/*
 * Free the queue and return 0 (success) or -1 (failure).
 */
int
queue_free (queue_t queue) {
    node_t n = NULL;
    node_t temp = NULL;
    if ( !(queue) ) {
        return -1;
    }

    n = queue->head;
    while (n) {
        temp = n->next;
        free(n);

        n = temp;
    }

    free(queue);

    return 0;
}

void
increment(void *data, void *acc) {
    *(int *)acc += 1;
}

/*
 * Return the number of items in the queue.
 */
int
queue_length(queue_t queue) {
    int len = 0;
    if ( !(queue) ) {
        return -1;
    }

    queue_iterate(queue, &increment, &len);

    return len;
}


/*
 * Delete the specified item from the given queue.
 * Return -1 on error.
 */
int
queue_delete(queue_t queue, void* item) {
    node_t prev = NULL;
    node_t n = NULL;
    if ( !(queue) ) {
        return -1;
    }

    n = queue->head;
    while (n) {
        if (n->data == item) {
            if (n == queue->head) {
                queue->head = n->next;
                if (n == queue->tail) {
                    queue->tail = NULL;
                }
            }

            else if (n == queue->tail) {
                queue->tail = prev;
                queue->tail->next = NULL;
            }

            else {
                prev->next = n->next;
            }

            free(n);
            return 0;
        }

        prev = n;
        n = n->next;
    }

    return -1;
}
