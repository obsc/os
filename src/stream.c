/*
 * Stream implementation
 */

#include "defs.h"
#include "network.h"
#include "queue.h"
#include "stream.h"
#include "synch.h"
#include "miniheader.h"

struct stream
{
    int index; //index at current message
    queue_t data;
    semaphore_t lock;
    //TO DO: maybe add num waiting 
};

/*
 * Return an empty stream. Returns Null on error
 */
 stream_t stream_new() {
    queue_t q;
    stream_t s;
    semaphore_t lock;
    s = (stream_t) malloc (sizeof(struct stream));
    lock = semaphore_create();
    q = queue_new();
    if (!q || !lock || !s) {
        free(s);
        free(q);
        free(lock);
        return NULL;
    }
    semaphore_initialize(lock, 1);
    s->data = q;
    s->index = 0;
    s->lock = lock;
    return s;
 }

 /*
  * Returns 1 if stream is empty, 0 if not, and -1 for error
  */
 int stream_is_empty(stream_t stream) {
    if (!stream) return -1;
    semaphore_P(stream->lock);
    if (queue_length(stream->data) == 0) {
        semaphore_V(stream->lock);
        return 1;
    }
    semaphore_V(stream->lock);
    return 0;
 }

/*
 * adds a network_interrupt_arg_t to the stream
 * Returns 0 (success) or -1 (failure)
 */
int stream_add(stream_t stream, network_interrupt_arg_t* next) {
    if (!stream || !next) return -1;
    semaphore_P(stream->lock);
    if (queue_append(stream->data, next) == 0) {
        semaphore_V(stream->lock);
        return 0;
    }
    semaphore_V(stream->lock);
    return -1;
}

/*
 * take data of size int from the stream and return
 * it in the given char *
 * Returns size of the returned char * (success)
 * or -1 (failure)
 */
int stream_take(stream_t stream, int request, char * output) {
    int start_read;
    void *node;
    int request_left;
    int size_current_node;
    int message_iterator;
    network_interrupt_arg_t *current_chunk;

    if (!stream || !output) return -1;

    message_iterator = 0;
    request_left = request;

    // stop when request is fulfuilled or stream is empty
    while (request_left != 0 && stream_is_empty(stream) == 0) {

        semaphore_P(stream->lock);
        start_read = sizeof(struct mini_header_reliable) + stream->index;
        queue_peek(stream->data, &node);
        semaphore_V(stream->lock);
        current_chunk = (network_interrupt_arg_t *) node;
        size_current_node = current_chunk->size - start_read;

        // if request is larger than this node's data size, we take all of the data
        // else we only take what we need
        if (request_left >= size_current_node) {
            memcpy(output + message_iterator, current_chunk->buffer + start_read, size_current_node);
            message_iterator = message_iterator + size_current_node;
            request_left = request_left - size_current_node;
            semaphore_P(stream->lock);
            stream->index = 0;
            queue_dequeue(stream->data, &node);
            semaphore_V(stream->lock);
            free(current_chunk);
        } else {
            memcpy(output + message_iterator, current_chunk->buffer + start_read, request_left);
            message_iterator = message_iterator + request_left;
            semaphore_P(stream->lock);
            stream->index = stream->index + request_left;
            semaphore_V(stream->lock);
            request_left = 0;
        }
    }

    return message_iterator;
}

/*
 * destroy and free the stream
 */
int stream_destroy(stream_t stream) {
    void * node;
    network_interrupt_arg_t *next;

    if ( !stream ) return -1;

    while (stream_is_empty(stream) == 0) {
        queue_dequeue(stream->data, &node);
        next = (network_interrupt_arg_t *) node;
        free(next);
    }
    semaphore_destroy(stream->lock);
    queue_free(stream->data);
    free(stream);
    return 0;
}
