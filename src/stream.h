/*
 * A stream implemented as a queue, where each
 * node contains a network_interrupt_arg_t
 */
#ifndef __STREAM_H__
#define __STREAM_H__

#include "network.h"

/*
 * stream_t is a pointer to an internally maintained stream
 */
typedef struct stream* stream_t;

/*
 * Return an empty stream. Returns Null on error
 */
extern stream_t stream_new();

/*
 * adds a network_interrupt_arg_t to the stream
 * Returns 0 (success) or -1 (failure)
 */
extern int stream_add(stream_t, network_interrupt_arg_t*);

/*
 * take data of size int from the stream and return
 * it in the given char *
 * Returns size of the returned char * (success)
 * or -1 (failure)
 */
extern int stream_take(stream_t, int, char *);

/*
 * destroy and free the stream
 */
extern int stream_destroy(stream_t);

 /*
  * Returns 1 if stream is empty, 0 if not
  */
extern int stream_is_empty(stream_t);

#endif  /* __STREAM_H__ */
