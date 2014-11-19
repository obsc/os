/*
 * Hashtable implementation.
 *
 */
#include "hashtable.h"
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

#define checkNull(q) if( !(q) ) { return -1; }

typedef struct tuple* tuple_t;

struct hashtable
{
    queue_t buckets;
    int max_size;
    int current_size;
};

struct tuple
{
    void * first;
    void * second;
};


/*
 * Return an empty hashtable of some specified size.  Returns NULL on error.
 */
hashtable_t hashtable_new(int size) {
    queue_t buckets;
    int iterator;
    int free_pointer;

    hashtable_t hashtable = (hashtable_t) malloc (sizeof(struct hashtable));
    buckets = (queue_t) malloc (sizeof(struct queue));
    if (buckets || !hashtable) {
        free(hashtable);
        free(buckets);
        return NULL;
    }

    for (iterator = 0; iterator < size; iterator++) {
        buckets[iterator] = queue_new();
        if (!buckets[iterator]) {
            for (free_pointer = iterator; free_pointer >= 0; free_pointer --) {
                free(buckets[free_pointer]);
            }
            free(hashtable);
            return NULL;
        }
    }

    hashtable->buckets = buckets;
    hashtable->max_size = size;
    hashtable->current_size = 0;

    return hashtable;

}

/*
 * returns the tuple of key value pair if hashtable contains the specified key,
 * else returns NULL
 */
tuple_t hashtable_contains(hashtable_t hashtable, void* key) {
    void * node;
    tuple_t result;
    int hashed_key;
    int checked;

    if (!hashtable || !key || !value) return NULL;
    checked = 0;
    //hashed_key = hash(key);
    while (checked < queue_length(hashtable->buckets[hashed_key])) {
        if (queue_dequeue(hashtable->buckets[hashed_key], &node) == 0) {
            result = (tuple_t) node;
            queue_append(hashtable->buckets[hashed_key], result);
            if (key == result->first) {
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
 * get the value of the specified key. Returns 0 (success) or -1 (failure)
 */
int hashtable_get(hashtable_t hashtable, void * key, void ** value) {
    tuple_t result;

    result = hashtable_contains(hashtable, key);
    if (!result) {
        return -1;
    }
    *value = result->second;
    return 0;
}

/*
 * Set the specified key as this value. Returns 0 (success) or -1 (failure)
 */
int hashtable_set(hashtable_t hashtable, void *key, void *value) {
    int hashed_key; // need to hash
    tuple_t new_kv;
    tuple_t found_kv;

    if (!hashtable || !key || !value) return -1;
    found_kv = hashtable_contains(hashtable, key);
    if (!found_kv) {
        //hashed_key = hash(key);
        new_kv = (tuple_t) malloc (sizeof(struct new_kv));
        if (!new_kv) return -1;

        new_kv->first = key;
        new_kv->second = value;
        if (queue_append(hashtable->buckets[hashed_key], new_kv) == -1) return -1;
        hashtable->current_size += 1;
    } else {
        found_kv->second = value;
    }


    return 0;

}

/*
 * delete the corresponding key from the hashtable. Returns 0 (success) or -1 (failure)
 */
int hashtable_delete(hashtable_t hashtable, void *key) {
    tuple_t found_kv;
    int hashed_key;

    found_kv = hashtable_contains(hashtable, key);
    if (!found_kv) return -1;
    hashtable->current_size --;
    return queue_delete(hashtable->buckets[hashed_key], found_kv);
}

/*
 * Free the hashtable and return 0 (success) or -1 (failure).
 */
int hashtable_free (hashtable_t hashtable) {
    int iterator;

    if (!hashtable) return -1;
    for (iterator = 0; iterator < max_size; iterator ++) {
        if (queue_free(hashtable->buckets[iterator] == -1)) {
            return -1;
        }
    }
    free(hashtable->buckets);
    free(hashtable);
    return 0;
}
