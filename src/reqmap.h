/*
 * Request Map Interface
 */
#ifndef __REQMAP_H__
#define __REQMAP_H__
/*
 * reqmap_t is a pointer to an internally maintained data structure.
 */
typedef struct reqmap* reqmap_t;

/*
 * Return an empty reqmap of some specified size.  Returns NULL on error.
 */
extern reqmap_t reqmap_new(int);

/*
 * Set the specified key (int, void *) as this value. Returns 0 (success) or -1 (failure)
 * Also returns the evicted value if applicable.
 */
extern int reqmap_set(reqmap_t, int, void *, void*);

/*
 * get the value of the specified key (int, void *). Returns 0 (success) or -1 (failure)
 */
extern int reqmap_get(reqmap_t, int, void *, void**);

/*
 * delete the corresponding key (int, void *) from the reqmap. Returns 0 (success) or -1 (failure)
 */
extern int reqmap_delete(reqmap_t, int, void *);

/*
 * Free the reqmap and return 0 (success) or -1 (failure).
 */
extern int reqmap_destroy (reqmap_t);

#endif /*__REQMAP_H__*/
