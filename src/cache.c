/*
 * FIFO Cache Implementation
 */
#include "cache.h"
#include <stdlib.h>
#include <stdio.h>
#include "miniroute.h"
#include "alarm.h"
#include "network.h"
#include "list.h"

typedef struct hashtable* hashtable_t;
typedef struct tuple* tuple_t;

struct cache
{
    list_t list;
    hashtable_t hashtable;
    int current_size;
    int max_size;
};


struct hashtable
{
    list_t *buckets;
    int max_size;
};

struct tuple
{
    network_address_t key;
    void * value;
    void * list_node;
    void * chain_node;
};


/*
 * Return an empty hashtable of some specified size.  Returns NULL on error.
 */
hashtable_t hashtable_new(int size) {
    int iterator;

    hashtable_t hashtable = (hashtable_t) malloc (sizeof(struct hashtable));
    if (!hashtable) {
        free(hashtable);
        return NULL;
    }
    hashtable->buckets = (list_t *) malloc (sizeof(list_t) * size);

    if (!hashtable->buckets) {
        free(hashtable);
        return NULL;
    }
    for (iterator = 0; iterator < size; iterator++) {
        (hashtable->buckets)[iterator] = list_new();
        if (!((hashtable->buckets)[iterator])) {
            break;
        }
    }

    if (iterator < size) {
        for (; iterator > 0; iterator--) {
            list_free((hashtable->buckets)[iterator-1]);
        }
        free(hashtable->buckets);
        free(hashtable);
        return NULL;
    }

    hashtable->max_size = size;

    return hashtable;

}

//void compare_keys(void* tuple, void* key, void** output) {
//	tuple_t t;
//	network_address_t addr;
//	t = (tuple_t) tuple;
//	addr = (unsigned int *) key;
//      addr = (network_address_t) addr;
//	if (network_compare_network_addresses(addr, t->key) != 0) {
//		*output = tuple;
//	}
//}

int hash_naive(network_address_t address, int size) {
	unsigned short unmodded;
	int result;

	unmodded = hash_address(address);

	result = ((int) unmodded) % size;

	return result;
}
/*
 * returns the tuple of k,v,l,c pair if hashtable contains the specified key,
 * else returns NULL
 */
tuple_t hashtable_contains(hashtable_t hashtable, network_address_t key) {
    tuple_t result;
    int hashed_key;
    void *node;
    int checked;

    if (!hashtable || !key) return NULL;
    hashed_key = hash_naive(key, hashtable->max_size);
  //  *output = NULL;
  //  input = (void *) key;
  //  list_iterate(hashtable->buckets[hashed_key], compare_keys, input, output);
  //  if ((*output) != NULL) {
  //  	result = (tuple_t) (*output);
  //  	return result;
  //  } else {
  //  	return NULL;
  //  }

     checked = 0;
     while (checked < list_length(hashtable->buckets[hashed_key])) {
         if (list_dequeue(hashtable->buckets[hashed_key], &node) == 0) {
             result = (tuple_t) node;
             list_append(hashtable->buckets[hashed_key], result);
             if (network_compare_network_addresses(key, result->key)) {
                 return result;
             }
             checked += 1;
         } else {
             return NULL;
         }
     }
     return NULL;
}

/*
 * get the tuple of the specified key. Returns 0 (success) or -1 (failure)
 */
int hashtable_get(hashtable_t hashtable, network_address_t key, void ** value) {
    tuple_t result;

    result = hashtable_contains(hashtable, key);
    if (!result) {
        return -1;
    }
    *value = result;
    return 0;
}

/*
 * Set the specified key as this tuple. Returns pointer to node (success) or NULL (failure)
 */
void * hashtable_set(hashtable_t hashtable, tuple_t tup) {
    int hashed_key; // need to hash
    tuple_t found_kv;
    void * node;

    if (!hashtable || !tup) return NULL;
    hashed_key = hash_naive(tup->key, hashtable->max_size);
    found_kv = hashtable_contains(hashtable, tup->key);
    if (!found_kv) {
        //hashed_key = hash(key);
    	node = list_append(hashtable->buckets[hashed_key], tup);
        if (node == NULL) return NULL;
        return node;
    } else {
        return NULL;
    }

}

/*
 * delete the corresponding key from the hashtable. Returns 0 (success) or -1 (failure)
 */
int hashtable_delete(hashtable_t hashtable, tuple_t tup) {
    int hashed_key;

    hashed_key = hash_naive(tup->key, hashtable->max_size);
    return list_delete(hashtable->buckets[hashed_key], tup->chain_node);
}

/*
 * Free the hashtable and return 0 (success) or -1 (failure).
 */
int hashtable_free (hashtable_t hashtable) {
    int iterator;

    if (!hashtable) return -1;
    for (iterator = 0; iterator < hashtable->max_size; iterator ++) {
        if (list_free(hashtable->buckets[iterator]) == -1) {
            return -1;
        }
    }
    free(hashtable->buckets);
    free(hashtable);
    return 0;
}



/*
 * Return an empty cache of some specified size.  Returns NULL on error.
 */
cache_t cache_new(int size) {
	hashtable_t h;
	list_t l;
	cache_t c;

	c = (cache_t) malloc (sizeof(struct cache));
	l = list_new();
	h = hashtable_new(size);

	if (!l || !c || !h) {
		free(l);
		free(c);
		free(h);
		return NULL;
	}

	c->list = l;
	c->hashtable = h;
	c->current_size = 0;
	c->max_size = size;
	return c;
}

/*
 * Set the specified key as this value. Returns 0 (success) or -1 (failure)
 */
int cache_set(cache_t cache, network_address_t key, void *value, void** output) {
	void *result;
	tuple_t tup;
	tuple_t evicted;
	void *l_node;
	void *c_node;

	if (hashtable_get(cache->hashtable, key, &result) == -1) {

		tup = (tuple_t) malloc (sizeof(struct tuple));
		if (!tup) return -1;
		network_address_copy(key, tup->key);
		tup->value = value;

		if (cache->current_size < cache->max_size) {
			l_node = list_append(cache->list, tup);
			if (!l_node) return -1;
			tup->list_node = l_node;
			c_node = hashtable_set(cache->hashtable, tup);
			if (!c_node) return -1;
			tup->chain_node = c_node;
			cache->current_size++;
                        *output = NULL;
			return 0;
		} else {
			if (list_dequeue(cache->list, &result) == 0) {
				evicted = (tuple_t) result;
                                *output = evicted->value;
				hashtable_delete(cache->hashtable, evicted);
				free(evicted);
				l_node = list_append(cache->list, tup);
				if (!l_node) return -1;
				tup->list_node = l_node;
				c_node = hashtable_set(cache->hashtable, tup);
				if (!c_node) return -1;
				tup->chain_node = c_node;
				return 0;
			} else {
                                *output = NULL;
				return -1;
			}
		}
	} else {
		tup = (tuple_t) result;
		tup->value = value;
                *output = NULL;
		return 0;
	}
}

/*
 * get the value of the specified key. Returns 0 (success) or -1 (failure)
 */
int cache_get(cache_t cache, network_address_t key, void ** output) {
	void *result;
	tuple_t tup;
	if (hashtable_get(cache->hashtable, key, &result) == 0) {
		tup = (tuple_t) result;
		*output = tup->value;
		return 0;
	}
	return -1;
}

/*
 * delete the corresponding key from the cache. Returns 0 (success) or -1 (failure)
 */
int cache_delete(cache_t cache, network_address_t key) {
	void *result;
	tuple_t tup;
	if (hashtable_get(cache->hashtable, key, &result) == 0) {
		tup = (tuple_t) result;
		cache->current_size --;
		if (hashtable_delete(cache->hashtable, tup) == -1) return -1;
		if (list_delete(cache->list, tup->list_node) == -1) return -1;
		free(tup);
		return 0;
	}
	return -1;
}

/*
 * Free the cache and return 0 (success) or -1 (failure).
 */
int cache_destroy (cache_t cache) {
	void *result;
	tuple_t tup;

	while (list_dequeue(cache->list, &result) == 0) {
		tup = (tuple_t) result;
		if (list_delete(cache->list, tup->list_node) == -1) return -1;
		free(tup);
	}
	if (list_free(cache->list) == -1) return -1;

	if (hashtable_free(cache->hashtable) == -1) return -1;
	free(cache);
	return 0;

}
