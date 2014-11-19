/*
 * FIFO Cache Implementation
 */
#include "hashtable.h"
#include "cache.h"
#include <stdlib.h>
#include <stdio.h>
#include "miniroute.h"
#include "alarm.h"
#include "queue.h"
#include "network.h"

typedef struct node* node_t;
typedef struct deleter* deleter_t;

struct node
{
	network_address_t key;
	char value[MAX_ROUTE_LENGTH][8];
	alarm_id alarm;
	deleter_t deleter;
	int length;
};

struct deleter
{
	node_t node;
	cache_t cache;
};

struct cache
{
	queue_t queue;
	hashtable_t hashtable;
	int current_size;
	int max_size;
};

/*
 * Return an empty cache of some specified size.  Returns NULL on error.
 */
cache_t cache_new(int size) {
	hashtable_t h;
	queue_t q;
	cache_t l;

	l = (cache_t) malloc (sizeof(struct cache));
	q = queue_new();
	h = hashtable_new(size);

	if (!l || !q || !h) {
		free(l);
		free(q);
		free(h);
		return NULL;
	}

	l->queue = q;
	l->hashtable = h;
	l->current_size = 0;
	l->max_size = size;
	return l;
}

void alarm_delete(void *value) {
	deleter_t d;
	d = (deleter_t) value;

	cache_delete(d->cache, d->node->key);
}
/*
 * Set the specified key as this value. Returns 0 (success) or -1 (failure)
 */
int cache_set(cache_t cache, network_address_t key, char (*value)[8], int len) {
	void *result;
	node_t n;
	deleter_t d;
	node_t evicted;
	int iterator;
	alarm_id new_alarm;

	if (hashtable_get(cache->hashtable, key, &result) == -1) {

		n = (node_t) malloc (sizeof(struct node));
		if (!n) return -1;
		n->key = key;
		for (iterator = 0; iterator < cache->max_size; iterator++) {
			memcpy(value[iterator], n->value[iterator], 8);
		}
		n->length = len;
		d = (deleter_t) malloc (sizeof(struct deleter));
		if (!d) return -1;
		d->cache = cache;
		d->node = n;
		new_alarm = register_alarm(3000, alarm_delete, (void *) d);
		n->alarm = new_alarm;
		n->deleter = d;
		if (cache->current_size < cache->max_size) {
			queue_append(cache->queue, n);
			hashtable_set(cache->hashtable, n);
			cache->current_size++;
			return 0;
		} else {
			if (queue_dequeue(cache->queue, &result) == 0) {
				evicted = (node_t) result;
				hashtable_delete(cache->hashtable, evicted->key);
				deregister_alarm(evicted->alarm);
				free(evicted->deleter);
				free(evicted);
				queue_append(cache->queue, n);
				hashtable_set(cache->hashtable, n);
				return 0;
			} else {
				return -1;
			}
		}
	} else {
		n = (node_t) result;
		for (iterator = 0; iterator < cache->max_size; iterator++) {
			memcpy(value[iterator], n->value[iterator], 8);
		}
		n->length = len;
		deregister_alarm(n->alarm);
		new_alarm = register_alarm(3000, alarm_delete, (void *) d);
		n->alarm = new_alarm;
		return 0;
	}
}

/*
 * get the value of the specified key. Returns length of path (success) or -1 (failure)
 */
int cache_get(cache_t cache, network_address_t key, char (*value)[8]) {
	void *result;
	node_t node;
	int iterator;
	if (hashtable_get(cache->hashtable, key, &result) == 0) {
		node = (node_t) result;
		for (iterator = 0; iterator < cache->max_size; iterator++) {
			memcpy(value[iterator], node->value[iterator], 8);
		}
		return node->length;
	}
	return -1;
}

/*
 * delete the corresponding key from the cache. Returns 0 (success) or -1 (failure)
 */
cache_delete(cache_t cache, network_address_t key) {
	void *result;
	node_t node;
	int iterator;
	if (hashtable_get(cache->hashtable, key, &result) == 0) {
		node = (node_t) result;
		hashtable_delete(cache->hashtable, key);
		
	}
	return -1;
}

/*
 * Free the cache and return 0 (success) or -1 (failure).
 */
extern int cache_destroy (cache_t);

