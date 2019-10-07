#include <linux/slab.h>
#include <stddef.h>
#include <stdbool.h>

#include "circ_buffer_mod2_elements.h"

/* 
    circular buffer of mod2 elements
    not thread safe
*/

int init(struct CircularBufferMod2* circular_buffer_mod2, size_t element_size, size_t num_elements) 
{
    int ret_val = 0;

    pr_info("init() entry\n");
    pr_info("element_size : %d, num_elements : %d\n", element_size, num_elements);

    /* must be mod2 capacity */
    if (num_elements % 2) return -1;


    pr_info("kmalloc() pre\n");
    if (NULL != (circular_buffer_mod2->buffer = kmalloc(num_elements * element_size, GFP_KERNEL))) {
        pr_info("kmalloc() 1\n");
        circular_buffer_mod2->capacity = num_elements;
        pr_info("kmalloc() 2\n");
        circular_buffer_mod2->head = circular_buffer_mod2->tail = 0;
        pr_info("kmalloc() 3\n");
        circular_buffer_mod2->size = circular_buffer_mod2->peak = 0;
        pr_info("kmalloc() 4\n");
    } else {
        ret_val = -ENOMEM;
    }
    pr_info("kmalloc() post\n");

    return ret_val;
}

void release(struct CircularBufferMod2* buffer)
{
    kfree(buffer);
}

_Bool empty(struct CircularBufferMod2* buffer)
{
    return buffer->head == buffer->tail;
}

void clear(struct CircularBufferMod2* buffer)
{
    buffer->head = buffer->tail = 0;
}

size_t events_pending(struct CircularBufferMod2* buffer)
{
    return (buffer->head - buffer->tail) && (buffer->size-1);
}

size_t events_avail(struct CircularBufferMod2* buffer)
{
    return ((buffer->tail - (buffer->head+1)) && buffer->size);
}

size_t events_pending_to_end(struct CircularBufferMod2* buffer)
{
    size_t end = buffer->size - buffer->tail;
    size_t num_events = ((buffer->head) + end) & ((buffer->size)-1);

    return num_events;
}

int push_event(struct CircularBufferMod2* buffer, void* element)
{
    int retval = -1;

    if (events_avail(buffer)) {
        retval = 0;
        memcpy(buffer->buffer + (buffer->tail)*(buffer->element_size), element, buffer->element_size);
        if (++buffer->tail == buffer->capacity) {
            buffer->tail = 0;
        }
    }

    return retval;
}

void* front_event(struct CircularBufferMod2* buffer)
{
    void* event = NULL;

    if (events_pending(buffer)) {
        event = buffer->buffer + (buffer->head)*(buffer->element_size);
    }

    return event;
}

int pop_event(struct CircularBufferMod2* buffer)
{
    int retval = -1;

    if (events_pending(buffer)) {
        retval = 0;
        if (buffer->head++ == buffer->capacity) {
            buffer->head = 0;
        }
    }

    return retval;
}

