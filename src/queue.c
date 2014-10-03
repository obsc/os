/*
 * Generic queue implementation.
 *
 */
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

#define checkNull(q) if( !(q) ) { return -1; }

typedef struct node* node_t;

struct node
{
    void *data;
    node_t next;
};

/*
 * Struct representing a queue backed by a singly-linked list
 */
struct queue
{
    node_t head;
    node_t tail;
    int length;
};

/*
 * Return an empty queue.
 */
queue_t
queue_new() {
    queue_t q = (queue_t) malloc (sizeof(struct queue));
    q->head = NULL;
    q->tail = NULL;
    q->length = 0;
    return q;
}

/*
 * Prepend a void* to a queue (both specifed as parameters).  Return
 * 0 (success) or -1 (failure).
 */
int
queue_prepend(queue_t queue, void* item) {
    node_t n = NULL;
    checkNull(queue);

    // Mallocs a new node
    n = (node_t) malloc (sizeof(struct node));
    checkNull(n);

    n->data = item;
    n->next = queue->head;
    // Checks if the queue is empty
    if ( queue->length == 0 ) {
        queue->tail = n;
    }

    queue->head = n;
    queue->length++;
    return 0;
}

/*
 * Append a void* to a queue (both specifed as parameters). Return
 * 0 (success) or -1 (failure).
 */
int
queue_append(queue_t queue, void* item) {
    node_t n = NULL;
    checkNull(queue);

    // Mallocs a new node
    n = (node_t) malloc (sizeof(struct node));
    checkNull(n);

    n->data = item;
    n->next = NULL;
    // Checks if queue is empty
    if ( queue->length == 0) {
        queue->head = n;
    } else {
        queue->tail->next = n;
    }

    queue->tail = n;
    queue->length++;
    return 0;
}

/*
 * Dequeue and return the first void* from the queue or NULL if queue
 * is empty.  Return 0 (success) or -1 (failure).
 */
int
queue_dequeue(queue_t queue, void** item) {
    node_t n = NULL;
    checkNull(queue);
    checkNull(item);

    // Checks if queue is empty
    if ( queue->length == 0 ) {
        *item = NULL;
        return -1;
    }

    n = queue->head;
    queue->head = n->next;

    // Checks if the queue had 1 element
    if ( queue->length == 1 ) {
        queue->tail = NULL;
    }

    // Frees node
    *item = n->data;
    free(n);

    queue->length--;
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
    // Checks if either queue or function is null
    checkNull(queue);
    checkNull(f);

    n = queue->head;
    // Iterates over queue
    while (n) {
        f(n->data, item); // Applies function

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
    node_t temp = NULL; // Used to keep track of next after freeing
    checkNull(queue);

    n = queue->head;
    // Iterates over queue
    while (n) {
        temp = n->next;
        free(n); // Frees each node

        n = temp;
    }

    free(queue); // Frees entire queue

    return 0;
}

/*
 * Return the number of items in the queue.
 */
int
queue_length(queue_t queue) {
    checkNull(queue);
    return queue->length;
}


/*
 * Delete the specified item from the given queue.
 * Return -1 on error.
 */
int
queue_delete(queue_t queue, void* item) {
    node_t prev = NULL;
    node_t n = NULL;
    checkNull(queue);

    n = queue->head;
    // Iterates over the queue
    while (n) {
        // Found the item
        if (n->data == item) {
            // Item found as first in queue
            if (n == queue->head) {
                queue->head = n->next;
                // Checks if queue only has 1 item
                if (n == queue->tail) {
                    queue->tail = NULL;
                }
            }

            // Item found last in queue (must have >= 2 items)
            else if (n == queue->tail) {
                queue->tail = prev;
                queue->tail->next = NULL;
            }

            // Item found in middle of queue (must have >= 3 items)
            else {
                prev->next = n->next;
            }

            // Frees the node
            free(n);
            queue->length--;
            return 0;
        }

        prev = n;
        n = n->next;
    }

    // Could not find item
    return -1;
}
