/*
 * Hashtable Interface
 */
#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

/*
 * hashtable_t is a pointer to an internally maintained data structure.
 */
typedef struct hashtable* hashtable_t;

/*
 * Return an empty hashtable of some specified size.  Returns NULL on error.
 */
extern hashtable_t hashtable_new(int);

/*
 * Set the specified key as this value. Returns 0 (success) or -1 (failure)
 */
extern int hashtable_set(hashtable_t, void *, void *);

/*
 * get the value of the specified key. Returns 0 (success) or -1 (failure)
 */
extern int hashtable_get(hashtable_t, void *, void **);

/*
 * delete the corresponding key from the hashtable. Returns 0 (success) or -1 (failure)
 */
extern hashtable_delete(hashtable_t, void *);

/*
 * Free the hashtable and return 0 (success) or -1 (failure).
 */
extern int hashtable_free (hashtable_t);

#endif /*__HASHTABLE_H__*/
