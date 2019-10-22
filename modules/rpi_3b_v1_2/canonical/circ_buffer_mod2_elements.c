#include <linux/slab.h>
#include <stddef.h>
#include <stdbool.h>

#include "circ_buffer_mod2_elements.h"
#include "util.h"

/* 
    circular buffer of mod2 elements
    not thread safe
*/

int init(struct CircularBufferMod2* circular_buffer_mod2, size_t element_size, size_t num_elements) 
{
    int ret_val = 0;

    PR_INFO("entry");
    PR_INFO("element_size : %d, num_elements : %d", element_size, num_elements);

    /* must be mod2 capacity */
    if (num_elements % 2) return -1;


    PR_INFO("kmalloc() pre");
    if (NULL != (circular_buffer_mod2->buffer = kmalloc(num_elements * element_size, GFP_KERNEL))) {
        circular_buffer_mod2->element_size = element_size;
        circular_buffer_mod2->capacity = num_elements;
        circular_buffer_mod2->head = 0;
        circular_buffer_mod2->tail = 0;
        circular_buffer_mod2->size = circular_buffer_mod2->peak = 0;
        PR_INFO("capacity : %d, head : %lld, tail : %lld", circular_buffer_mod2->capacity, circular_buffer_mod2->head, circular_buffer_mod2->tail);
    } else {
        ret_val = -ENOMEM;
    }
    PR_INFO("kmalloc() post");

    return ret_val;
}

void release(struct CircularBufferMod2* circular_buffer_mod2)
{
    kfree(circular_buffer_mod2->buffer);
}

_Bool empty(struct CircularBufferMod2* buffer)
{
    PR_INFO("entry");

    return buffer->head == buffer->tail;
}

void clear(struct CircularBufferMod2* buffer)
{
    PR_INFO("entry");

    buffer->head = 0;
    buffer->tail = 0;
}

// size_t events_pending(struct CircularBufferMod2* buffer)
size_t size(struct CircularBufferMod2* buffer)
{
    PR_INFO("entry");

    return (buffer->head - buffer->tail) & (buffer->size-1);
}

// size_t events_avail(struct CircularBufferMod2* buffer)
size_t space(struct CircularBufferMod2* buffer)
{
    size_t space = ((buffer->tail - (buffer->head+1)) & (buffer->size-1));
    PR_INFO("capacity : %d, space : %zu, head : %lld, tail : %lld", buffer->capacity, space, buffer->head, buffer->tail);

    return ((buffer->tail - (buffer->head+1)) & (buffer->size-1));
}

size_t events_pending_to_end(struct CircularBufferMod2* buffer)
{

    int64_t end = buffer->size - buffer->tail;
    int64_t num_events = ((buffer->head) + end) & ((buffer->size)-1);

    PR_INFO("entry");

    return num_events;
}

int push(struct CircularBufferMod2* buffer, void* element)
{
    int retval = -1;

    PR_INFO("entry");

    if (space(buffer)) {
        retval = 0;
        memcpy(buffer->buffer + (buffer->head)*(buffer->element_size), element, buffer->element_size);
        if (++(buffer->head) == buffer->capacity) {
            PR_INFO("head wrapped");
            buffer->head = 0;
        }
        PR_INFO("capacity : %d, head : %lld, tail : %lld", buffer->capacity, buffer->head, buffer->tail);
    } else {
        PR_ERR("buffer is full");
    }

    return retval;
}

void* front(struct CircularBufferMod2* buffer)
{
    void* event = NULL;

    PR_INFO("entry");

    if (size(buffer)) {
        event = buffer->buffer + (buffer->tail)*(buffer->element_size);
    }

    return event;
}

void pop(struct CircularBufferMod2* buffer)
{
    PR_INFO("entry");

    if (size(buffer)) {
        if (buffer->tail++ == buffer->capacity) {
            buffer->tail = 0;
        }
    }
}

