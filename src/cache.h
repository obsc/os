/*
 * Cache Interface
 */
#ifndef __CACHE_H__
#define __CACHE_H__

/*
 * cache_t is a pointer to an internally maintained data structure.
 */
typedef struct cache* cache_t;

/*
 * Return an empty cache of some specified size.  Returns NULL on error.
 */
extern cache_t cache_new(int);

/*
 * Set the specified key as this value. Returns 0 (success) or -1 (failure)
 */
extern int cache_set(cache_t, network_address_t, void*, void**);

/*
 * get the value of the specified key. Returns length of path (success) or -1 (failure)
 */
extern int cache_get(cache_t, network_address_t, void**);

/*
 * delete the corresponding key from the cache. Returns 0 (success) or -1 (failure)
 */
extern int cache_delete(cache_t, network_address_t);

/*
 * Free the cache and return 0 (success) or -1 (failure).
 */
extern int cache_destroy (cache_t);

#endif /*__CACHE_H__*/
