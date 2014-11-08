/*
 * Stream implementation
 */

#include "queue.h"
#include "stream.h"
#include "synch.h"
#include "miniheader.h"
#include <stdlib.h>
#include <stdio.h>

struct stream
{
    int index; //index at current message
    queue_t data;
    semaphore_t has_data;
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
    semaphore_t has_data;
    s = (stream_t) malloc (sizeof(struct stream));
    lock = semaphore_create();
    has_data = semaphore_create();
    q = queue_new();
    if (!q || !lock || !has_data || !s) {
        free(s);
        free(q);
        free(lock);
        free(has_data);
        return NULL;
    }
    semaphore_initialize(lock, 1);
    semaphore_initialize(has_data, 0);
    s->data = q;
    s->index = 0;
    s->has_data = has_data;
    s->lock = lock;
    return s;
 }

 /*
  * Returns 1 if stream is empty, 0 if not
  */
 int isEmpty(stream_t stream) {
    if (queue_length(stream->data) == 0) {
        return 1;
    }
    return 0;
 }

/*
 * adds a network_interrupt_arg_t to the stream
 * Returns 0 (success) or -1 (failure)
 */
int stream_add(stream_t stream, network_interrupt_arg_t* next) {
    semaphore_P(stream->lock);
    if (queue_enqueue(stream, next) == 0) {
        if (isEmpty(stream) == 0) {
            semaphore_V(stream->has_data);
        }
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
    char *message[request];
    void *node;
    int request_left;
    int size_current_node;
    int message_iterator;
    network_interrupt_arg_t *current_chunk;

    semaphore_P(stream->has_data);
    semaphore_P(stream->lock);
    message_iterator = 0;
    request_left = request;

    while (request_left != 0 && isEmpty(stream) == 0) {

        queue_peek(stream->data, &node);
        current_chunk = (network_interrupt_arg_t *) node;
        start_read = sizeof(mini_header_reliable) + stream->index;
        size_current_node = current_chunk->size - start_read;

        if (request_left >= size_current_node) {
            memcpy(message + message_iterator, current_chunk->buffer + start_read, size_current_node);
            message_iterator = message_iterator + size_current_node;
            stream->index = 0;
            request_left = request_left - size_current_node;
            queue_dequeue(stream, &node);
            free(current_chunk);
        } else {
            memcpy(message + message_iterator, current_chunk->buffer + start_read, request_left);
            message_iterator = message_iterator + request_left;
            stream->index = stream->index + request_left;
            request_left = 0;
        }
    }

    output = message;
    if (isEmpty(stream) == 0) {
        semaphore_V(stream->has_data);
    }
    semaphore_V(stream->lock);

    return message_iterator;
}

/*
 * destroy and free the stream
 */
int stream_destroy(stream_t stream) {
    void * node;
    network_interrupt_arg_t *next;
    while (isEmpty(stream) == 0) {
        queue_dequeue(stream->data, &node);
        next = (network_interrupt_arg_t *) node;
        free(next);
    }
    semaphore_destroy(stream->lock);
    semaphore_destroy(stream->has_data);
    queue_free(stream->data);
    free(stream);
}