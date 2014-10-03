/*
 * Multilevel queue manipulation functions  
 */
#include "multilevel_queue.h"
#include <stdlib.h>
#include <stdio.h>

#define checkNull(q) if( !(q) ) { return -1; }


struct multilevel_queue
{
	queue_t queues[];
	int levels;
	int length;
}
/*
 * Returns an empty multilevel queue with number_of_levels levels. On error should return NULL.
 */
multilevel_queue_t multilevel_queue_new(int number_of_levels)
{
	int acc;
	multilevel_queue_t q;
	queue_t queues[number_of_levels]; 
	acc = 0;
	q = (multilevel_queue_t) malloc (sizeof(struct multilevel_queue));
    q->levels = number_of_levels;
    q->length = 0;
    while (acc < number_of_levels) {
    	queues[acc] = queue_new();
    	acc++;
    }
    q->queues = queues;
    return q;
}

/*
 * Appends an void* to the multilevel queue at the specified level. Return 0 (success) or -1 (failure).
 */
int multilevel_queue_enqueue(multilevel_queue_t queue, int level, void* item)
{
	checkNull(queue);
	checkNull(item);
	if (level >= queue->levels) {
		return -1;
	}
	queue->length++;
	return queue_enqueue(((queue->queues)[level]), item);
}

/*
 * Dequeue and return the first void* from the multilevel queue starting at the specified level. 
 * Levels wrap around so as long as there is something in the multilevel queue an item should be returned.
 * Return the level that the item was located on and that item if the multilevel queue is nonempty,
 * or -1 (failure) and NULL if queue is empty.
 */
int multilevel_queue_dequeue(multilevel_queue_t queue, int level, void** item)
{
	int current_level;
	int acc;
	checkNull(queue);
	checkNull(item);
	current_level = level % (queue->levels);
	acc = 0;
	while (acc < queue->levels) {
		if (queue_dequeue(((queue->queues)[current_level]), item) == 0) {
			queue->length--;
			return current_level;
		}
		current_level = (current_level + 1) % (queue->levels);
		acc++;
	}
	*item = NULL;
	return -1;

}

/* 
 * Free the queue and return 0 (success) or -1 (failure). Do not free the queue nodes; this is
 * the responsibility of the programmer.
 */
int multilevel_queue_free(multilevel_queue_t queue)
{
	int acc;
	checkNull(queue);
	acc = 0;
	while (acc < queue->levels) {
		free(((queue->queues)[acc]));
	}
	free(queue);
	return 0;
}

/* returns the sum of items in the queues of the multilevel queue */
int multilevel_queue_length(multilevel_queue_t queue)
{
	return queue->length;
}
