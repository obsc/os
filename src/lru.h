/*
 * Least Recently Used Cache Interface
 */
#ifndef __LRU_H__
#define __LRU_H__

/*
 * lru_t is a pointer to an internally maintained data structure.
 */
typedef struct lru* lru_t;

/*
 * Return an empty lru of some specified size.  Returns NULL on error.
 */
extern lru_t lru_new(int);

/*
 * Set the specified key as this value. Returns 0 (success) or -1 (failure)
 */
extern int lru_set(lru_t, network_address_t, char[MAX_ROUTE_LENGTH][8]);

/*
 * get the value of the specified key. Returns 0 (success) or -1 (failure)
 */
extern int lru_get(lru_t, network_address_t, char[MAX_ROUTE_LENGTH][8]);

/*
 * delete the corresponding key from the lru. Returns 0 (success) or -1 (failure)
 */
extern int lru_delete(lru_t, network_address_t);

/*
 * Free the lru and return 0 (success) or -1 (failure).
 */
extern int lru_destroy (lru_t);

#endif /*__LRU_H__*/
