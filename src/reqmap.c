/*
 * A Map of Request to Data structures
 */
#include "reqmap.h"
#include <stdlib.h>
#include <stdio.h>
#include "list.h"

#define TABLE_MULT 2

typedef struct tuple* tuple_t;

struct reqmap
{
    list_t *buckets;
    int max_size;
};

struct tuple
{
    int blockid;
    void * buffer;
    void * value;
    void * node;
};


/*
 * Return an empty reqmap of some specified size.  Returns NULL on error.
 */
reqmap_t reqmap_new(int size) {
    int iterator;

    reqmap_t reqmap = (reqmap_t) malloc (sizeof(struct reqmap));
    if (!reqmap) {
        free(reqmap);
        return NULL;
    }
    reqmap->buckets = (list_t *) malloc (sizeof(list_t) * TABLE_MULT * size);

    if (!reqmap->buckets) {
        free(reqmap);
        return NULL;
    }
    for (iterator = 0; iterator < TABLE_MULT * size; iterator++) {
        (reqmap->buckets)[iterator] = list_new();
        if (!((reqmap->buckets)[iterator])) {
            break;
        }
    }

    if (iterator < TABLE_MULT * size) {
        for (; iterator > 0; iterator--) {
            list_free((reqmap->buckets)[iterator-1]);
        }
        free(reqmap->buckets);
        free(reqmap);
        return NULL;
    }

    reqmap->max_size = size;

    return reqmap;

}

//void compare_keys(void* tuple, void* key, void** output) {
//  tuple_t t;
//  network_address_t addr;
//  t = (tuple_t) tuple;
//  addr = (unsigned int *) key;
//      addr = (network_address_t) addr;
//  if (network_compare_network_addresses(addr, t->key) != 0) {
//      *output = tuple;
//  }
//}

int hash_function(int value, int size) {
    int result;

    result = value % (TABLE_MULT * size);

    return result;
}
/*
 * returns the tuple of k,v,l,c pair if reqmap contains the specified key,
 * else returns NULL
 */
tuple_t reqmap_contains(reqmap_t reqmap, int blockid, void *buffer) {
    tuple_t result;
    int hashed_key;
    void *node;
    // int checked;
      //  *output = NULL;
  //  input = (void *) key;
  //  list_iterate(reqmap->buckets[hashed_key], compare_keys, input, output);
  //  if ((*output) != NULL) {
  //    result = (tuple_t) (*output);
  //    return result;
  //  } else {
  //    return NULL;
  //  }

    if (!reqmap || !buffer) return NULL;
    hashed_key = hash_function(blockid, reqmap->max_size);
    node = list_head(reqmap->buckets[hashed_key]);
    while (node) {
        result = (tuple_t) node_value(node);
        if (blockid == result->blockid && buffer == result->buffer) {
            return result;
        }
        node = node_next(node);
    }
    return NULL;


    // checked = 0;
    // while (checked < list_length(reqmap->buckets[hashed_key])) {
    //     if (list_dequeue(reqmap->buckets[hashed_key], &node) == 0) {
    //         result = (tuple_t) node;
    //         list_append(reqmap->buckets[hashed_key], result);
    //         if (network_compare_network_addresses(key, result->key)) {
    //             return result;
    //         }
    //         checked += 1;
    //     } else {
    //         return NULL;
    //     }
    // }
    // return NULL;
}

/*
 * get the value of the specified key (int, void *). Returns 0 (success) or -1 (failure)
 */
int reqmap_get(reqmap_t reqmap, int blockid, void *buffer, void **value) {
    tuple_t result;

    result = reqmap_contains(reqmap, blockid, buffer);
    if (!result) {
        return -1;
    }
    *value = result->value;
    return 0;
}

/*
 * Set the specified key (int, void *) as this value. Returns 0 (success) or -1 (failure)
 */
int reqmap_set(reqmap_t reqmap, int blockid, void *buffer, void *value) {
    int hashed_key; // need to hash
    tuple_t found_kv;
    void * node;
    tuple_t new_kv;

    if (!reqmap || !buffer || !value) return -1;
    hashed_key = hash_function(blockid, reqmap->max_size);
    found_kv = reqmap_contains(reqmap, blockid, buffer);
    if (!found_kv) {
        //hashed_key = hash(key);
        new_kv = (tuple_t) malloc (sizeof(struct tuple));
        new_kv->blockid = blockid;
        new_kv->buffer = buffer;
        new_kv->value = value;
        node = list_append(reqmap->buckets[hashed_key], new_kv);
        new_kv->node = node;
        return 0;
    } else {
        found_kv->value = value;
        return 0;
    }
    return -1;

}

/*
 * delete the corresponding key (int, void *) from the reqmap. Returns 0 (success) or -1 (failure)
 */
int reqmap_delete(reqmap_t reqmap, int blockid, void *buffer) {
    int hashed_key;
    tuple_t tup;

    hashed_key = hash_function(blockid, reqmap->max_size);
    tup = reqmap_contains(reqmap, blockid, buffer);
    if (!tup) return -1;
    if (list_delete(reqmap->buckets[hashed_key], tup->node) == -1) return -1;
    free(tup);
    return 0;
}

/*
 * Free the reqmap and return 0 (success) or -1 (failure).
 */
int reqmap_free (reqmap_t reqmap) {
    int iterator;

    if (!reqmap) return -1;
    for (iterator = 0; iterator < TABLE_MULT * reqmap->max_size; iterator ++) {
        if (list_free(reqmap->buckets[iterator]) == -1) {
            return -1;
        }
    }
    free(reqmap->buckets);
    free(reqmap);
    return 0;
}

