/*
 * doubly linked list implementation
 */
#include <stdlib.h>
#include <stdio.h>
#include "list.h"

#define checkNull(q) if( !(q) ) { return -1; }

typedef struct list_node* list_node_t;

struct list_node
{
	void *data;
	list_node_t next;
	list_node_t prev;
};

struct list
{
	list_node_t head;
	list_node_t tail;
	int length;
};

/*
 * Return an empty list.  Returns NULL on error.
 */
list_t list_new() {
	list_t l = (list_t) malloc (sizeof(struct list));
    if ( !l ) return NULL;

    l->head = NULL;
    l->tail = NULL;
    l->length = 0;
    return l;
}

/*
 * Appends a void* to a list (both specifed as parameters).  Return
 * pointer to node (success) or NULL (failure).
 */
void *
list_append(list_t list, void* item) {
    list_node_t n = NULL;
    if (!list) return NULL;

    // Mallocs a new node
    n = (list_node_t) malloc (sizeof(struct list_node));
    if (!n) return NULL;

    n->data = item;
    n->next = NULL;
    n->prev = NULL;
    // Checks if queue is empty
    if ( list->length == 0) {
        list->head = n;
    } else {
        list->tail->next = n;
        n->prev = list->tail;
    }

    list->tail = n;
    list->length++;
    return n;
}

/*
 * Delist and return the first void* from the list.
 * Return 0 (success) and first item if list is nonempty, or -1 (failure) and
 * NULL if list is empty.
 */
int
list_dequeue(list_t list, void** item) {
    list_node_t n = NULL;
    if (item == NULL) {
        return -1;
    }
    if (list == NULL) {
        *item = NULL;
        return -1;
    }

    // Checks if list is empty
    if ( list->length == 0 ) {
        *item = NULL;
        return -1;
    }
    n = list->head;
    list->head = n->next;
    if (list->head) {
        list->head->prev = NULL;
    }
    // Checks if the list had 1 element
    if ( list->length == 1 ) {
        list->tail = NULL;
    }

    // Frees node
    *item = n->data;
    free(n);
    list->length--;
    return 0;
}

/*
 * Free the list and return 0 (success) or -1 (failure).
 */
int
list_free (list_t list) {
    list_node_t n = NULL;
    list_node_t temp = NULL; // Used to keep track of next after freeing
    checkNull(list);

    n = list->head;
    // Iterates over queue
    while (n) {
        temp = n->next;
        free(n); // Frees each node

        n = temp;
    }

    free(list); // Frees entire queue

    return 0;
}

/*
 * list_iterate(q, f, t, o) calls f(x,t,o) for each x in q.
 * q and f should be non-null.
 *
 * returns 0 (success) or -1 (failure)
 */
int
list_iterate(list_t list, func_t f, void* arg, void** output) {
	list_node_t n = NULL;

	checkNull(list);
	checkNull(f);

	n = list->head;

	while (n) {
		f(n->data, arg, output);

		n = n->next;
	}

	return 0;
}

/*
 * Return the number of items in the list, or -1 if an error occured
 */
int
list_length(list_t list) {
	if (!list) return -1;
	return list->length;
}

/*
 * Delete the instance of item from the given list.
 * Returns 0 if an element was deleted, or -1 otherwise.
 */
int
list_delete(list_t list, void *node) {
	list_node_t n;

	if (!list || !node) return -1;
	n = (list_node_t) node;

    if (n->prev) {
        n->prev->next = n->next;
    }
    if (n->next) {
        n->next->prev = n->prev;
    }
	if (list->head == n) {
        list->head = n->next;
    }
    if (list->tail == n) {
        list->tail = n->prev;
    }
	free(n);
    list->length --;

	return 0;

}
