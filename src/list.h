/*
 * Doubly Linked List Interface (For private use only)
 */
#ifndef __LIST_H__
#define __LIST_H__

/*
 * list_t is a pointer to an internally maintained data structure.
 * Clients of this package do not need to know how lists are
 * represented.  They see and manipulate only list_t's.
 */
typedef struct list* list_t;

/*
 * Return an empty list.  Returns NULL on error.
 */
extern list_t list_new();

/*
 * Appends a void* to a list (both specifed as parameters).  Return
 * 0 (success) or -1 (failure).
 */
extern void *list_append(list_t, void*);

/*
 * Dequeue and return the first void* from the list.
 * Return 0 (success) and first item if list is nonempty, or -1 (failure) and
 * NULL if list is empty.
 */
extern int list_dequeue(list_t, void**);

/*
 * Free the list and return 0 (success) or -1 (failure).
 */
extern int list_free (list_t);

/*
 * Return the number of items in the list, or -1 if an error occured
 */
extern int list_length(list_t list);

/*
 * Delete the instance of item from the given list.
 * Returns 0 if an element was deleted, or -1 otherwise.
 */
extern int list_delete(list_t list, void* item);

/*
 * Get the first element of the list, returns NULL on failure
 */
extern void *list_head(list_t);

/*
 * Get the value of the node, returns NULL on failure
 */
extern void *node_value(void *);

/*
 * Get the node's next element, returns NULL on failure
 */
extern void *node_next(void *);


#endif /*__LIST_H__*/
