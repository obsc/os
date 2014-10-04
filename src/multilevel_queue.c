/*
 * Multilevel queue manipulation functions
 */
#include "multilevel_queue.h"
#include <stdlib.h>
#include <stdio.h>

#define checkNull(q) if( !(q) ) { return -1; }

struct multilevel_queue {
    int levels;
    int length;
    queue_t *queues;
};

/*
 * Returns an empty multilevel queue with number_of_levels levels. On error should return NULL.
 */
multilevel_queue_t multilevel_queue_new(int number_of_levels) {
    int acc;
    multilevel_queue_t q;

    q = (multilevel_queue_t) malloc (sizeof(struct multilevel_queue));
    if ( !q ) return NULL; // Failed to malloc

    q->levels = number_of_levels;
    q->length = 0;
    q->queues = (queue_t *) malloc (sizeof(queue_t) * number_of_levels);
    // if error on malloc'ing queues field, return NULL
    if ( !(q->queues) ) {
        free(q);
        return NULL;
    }

    // Initialize all the queues
    for (acc = 0; acc < number_of_levels; acc++) {
        (q->queues)[acc] = queue_new();
        if ( !((q->queues)[acc]) ) {
            break;
        }
    }

    // if there was an error malloc'ing all the queues, return NULL
    if (acc < number_of_levels) {
        for (; acc > 0; acc--) {
            queue_free((q->queues)[acc - 1]);
        }
        free(q->queues);
        free(q);
        return NULL;
    }

    return q;
}
/*
 * Appends an void* to the multilevel queue at the specified level.
 * Return 0 (success) or -1 (failure).
 */
int multilevel_queue_enqueue(multilevel_queue_t queue, int level, void* item) {
    checkNull(queue);
    checkNull(item);
    // Queue level out of bounds
    if (current_level < 0 || level >= queue->levels) {
        return -1;
    }
    queue->length++;
    return queue_append((queue->queues)[level], item);
}

/*
 * Dequeue and return the first void* from the multilevel queue starting at the specified level.
 * Levels wrap around so as long as there is something in the multilevel queue an item should be returned.
 * Return the level that the item was located on and that item if the multilevel queue is nonempty,
 * or -1 (failure) and NULL if queue is empty. Return -1 and NULL if level passed in is out of bounds
 */
int multilevel_queue_dequeue(multilevel_queue_t queue, int level, void** item) {
    int current_level;
    int acc;
    if (queue == NULL || item == NULL) {
        *item = NULL;
        return -1;
    }

    current_level = level;
    
    // Queue level out of bounds
    if (current_level < 0 || current_level >= queue->levels) {
    	*item = NULL;
    	return -1;
    }

    // Check levels starting at input level
    for (acc = 0; acc < queue->levels; acc++) {
        if (queue_dequeue(((queue->queues)[current_level]), item) == 0) {
            queue->length--;
            return current_level;
        }
        // wraparound if at the end of the levels
        current_level = (current_level + 1) % (queue->levels);
    }

    // no item found
    *item = NULL;
    return -1;
}

/*
 * Free the queue and return 0 (success) or -1 (failure). Do not free the queue nodes; this is
 * the responsibility of the programmer.
 */
int multilevel_queue_free(multilevel_queue_t queue) {
    int acc;
    checkNull(queue);

    for (acc = 0; acc < queue->levels; acc++) {
        queue_free((queue->queues)[acc]);
    }
    free(queue->queues);
    free(queue);
    return 0;
}

/* returns the sum of items in the queues of the multilevel queue */
int multilevel_queue_length(multilevel_queue_t queue) {
    checkNull(queue);
    return queue->length;
}
