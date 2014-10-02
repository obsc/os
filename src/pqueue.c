/*
 * Generic priority queue implementation.
 *
 */
#include "pqueue.h"
#include <stdlib.h>
#include <stdio.h>

#define checkNull(q) if( !(q) ) { return -1; }

typedef struct node* node_t;

/*
 * Struct representing a node in the priority queue
 */
struct node
{
    void *data // data
    int priority; // Priority of the alarm
    node_t next; // Pointer to next node
};

/*
 * Struct representing a priority queue backed by a singly-linked list
 */
struct pqueue
{
    node_t head;
    int length;
};

/*
 * Return an empty priority queue.  Returns NULL on error.
 */
pqueue_t
pqueue_new() {
    // pqueue_t q = (pqueue_t) malloc (sizeof(struct pqueue));
    // q->head = NULL;
    // q->length = 0;
    // return q;
    return NULL;
}

/*
 * Enqueues a void* to a priority queue (both specifed as parameters).
 * Returns 0 (success) or -1 (failure).
 */
int
pqueue_enqueue(pqueue_t pqueue, void *data, int priority) {
    // node_t n = NULL;
    // node_t check = NULL;
    // node_t prev = NULL;
    // checkNull(pqueue);

    // // Mallocs a new node
    // n = (node_t) malloc (sizeof(struct node));
    // checkNull(n);

    // n->data = data;
    // n->priority = priority;
    // n->next = NULL;
    // check = pqueue->head;
    // if (check == NULL) {
    //     pqueue->head = n;
    // } else {
    //     while (check != NULL) {
    //         if (priority < check->priority) {
    //             if (prev != NULL) {
    //                 prev->next = n;
    //             }
    //             n->next = check;
    //             if (check == pqueue->head) {
    //                 pqueue->head = n;
    //             }
    //             return 0;
    //         }
    //         prev = check;
    //         check = prev->next;
    //     }
    //     prev->next = n;
    // }
    
    // pqueue->length++;
    return 0;
}

/*
 * Dequeue and return the first node from the priority queue.
 * Return 0 (success) and first item if pqueue is nonempty, or -1 (failure) and
 * NULL if pqueue is empty.
 */
int
pqueue_dequeue(pqueue_t pqueue, void **data) {
    // node_t n = NULL;
    // checkNull(pqueue);
    // checkNull(data);

    // // Checks if queue is empty
    // if ( pqueue->length == 0 ) {
    //     *data = NULL;
    //     return -1;
    // }

    // n = pqueue->head;
    // pqueue->head = n->next;

    // // Frees node
    // *data = n->data;
    // free(n);

    // pqueue->length--;
    return 0;
}

/*
 * Return the first node from the priority queue without dequeueing.
 * Return 0 (success) and first item if queue is nonempty, or -1 (failure) and
 * NULL if pqueue is empty.
 */
int
pqueue_peek(pqueue_t pqueue, void **data) {
    // node_t n = NULL;
    // checkNull(pqueue);
    // checkNull(data);

    // // Checks if queue is empty
    // if ( pqueue->length == 0 ) {
    //     *data = NULL;
    //     return -1;
    // }

    // n = queue->head;
    // *data = n->data;

    return 0;
}

/*
 * Free the priority queue and return 0 (success) or -1 (failure).
 */
int
pqueue_free (pqueue_t pqueue) {
    // node_t n = NULL;
    // node_t temp = NULL; // Used to keep track of next after freeing
    // checkNull(pqueue);

    // n = pqueue->head;
    // // Iterates over queue
    // while (n) {
    //     temp = n->next;
    //     free(n); // Frees each node

    //     n = temp;
    // }

    // free(pqueue); // Frees entire queue

    return 0;
}

/*
 * Return the number of items in the priority queue, or -1 if an error occured
 */
int
pqueue_length(pqueue_t pqueue) {
    // checkNull(pqueue);
    // return pqueue->length;
    return 0;
}


/*
 * Delete the first instance of the specified item from the given queue.
 * Returns 0 if an element was deleted, or -1 otherwise.
 */
int
pqueue_delete(pqueue_t pqueue, void *data) {
    // node_t prev = NULL;
    // node_t n = NULL;
    // checkNull(pqueue);

    // n = pqueue->head;
    // // Iterates over the queue
    // while (n) {
    //     // Found the item
    //     if (n->data == item) {
    //         // Item found as first in queue
    //         if (n == pqueue->head) {
    //             pqueue->head = n->next;
    //         }

    //         // Item found last in queue (must have >= 2 items)
    //         else if (n->next == NULL) {
    //             prev->next = NULL;
    //         }

    //         // Item found in middle of queue (must have >= 3 items)
    //         else {
    //             prev->next = n->next;
    //         }

    //         // Frees the node
    //         free(n);
    //         pqueue->length--;
    //         return 0;
    //     }

    //     prev = n;
    //     n = n->next;
    // }

    // Could not find item
    return -1;
}
