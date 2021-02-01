#include <linux/circ_buf.h>
#include <linux/log2.h>
#include <linux/slab.h>
#include <stddef.h>
#include <stdbool.h>

#include "circ_buffer_pow2_elements.h"
#include "util.h"

/* 
    circular buffer of mod2 elements
    not thread safe
*/

int init(struct CircularBufferPow2* circular_buffer_mod2, size_t element_size, size_t num_elements) 
{
    int ret_val = 0;

    PR_INFO("entry");
    PR_INFO("element_size : %ld, num_elements : %ld", element_size, num_elements);

    /* must be pow 2 capacity */
    if ((num_elements == 0) || ((num_elements & (num_elements - 1)) != 0)) return -1;

    PR_INFO("kmalloc() pre, num_elements: %ld, element_size: %lu", num_elements, element_size);
    if (NULL != (circular_buffer_mod2->buffer = kmalloc(num_elements * element_size, GFP_KERNEL))) {
        circular_buffer_mod2->element_size = element_size;
        circular_buffer_mod2->capacity = num_elements;
        circular_buffer_mod2->head = 0;
        circular_buffer_mod2->tail = 0;
        circular_buffer_mod2->size = circular_buffer_mod2->peak = 0;
        PR_INFO("capacity : %ld, head : %d, tail : %d", circular_buffer_mod2->capacity, circular_buffer_mod2->head, circular_buffer_mod2->tail);
    } else {
        ret_val = -ENOMEM;
    }
    PR_INFO("kmalloc() post");

    return ret_val;
}

void release(struct CircularBufferPow2* circular_buffer_mod2)
{
    kfree(circular_buffer_mod2->buffer);
}

_Bool empty(struct CircularBufferPow2* buffer)
{
    PR_INFO("entry");

    return buffer->head == buffer->tail;
}

void clear(struct CircularBufferPow2* buffer)
{
    PR_INFO("entry");

    buffer->head = 0;
    buffer->tail = 0;
}

size_t size(struct CircularBufferPow2* buffer)
{
    int head = READ_ONCE(buffer->head);
    int tail = READ_ONCE(buffer->tail);

    PR_INFO("entry");
    return (head - tail) & (buffer->capacity-1);
}

size_t space(struct CircularBufferPow2* buffer)
{
    int head = READ_ONCE(buffer->head);
    int tail = READ_ONCE(buffer->tail);
    size_t space = CIRC_CNT(tail, head+1, buffer->capacity);

    PR_INFO("capacity : %ld, space : %zu, head : %d, tail : %d", buffer->capacity, space, buffer->head, buffer->tail);

    return space;
}

size_t events_pending_to_end(struct CircularBufferPow2* buffer)
{

    int64_t end = buffer->size - buffer->tail;
    int64_t num_events = ((buffer->head) + end) & ((buffer->size)-1);

    PR_INFO("entry");

    return num_events;
}

int push(struct CircularBufferPow2* buffer, void* element)
{
    int retval = 0;
    int head = READ_ONCE(buffer->head);
    int tail = READ_ONCE(buffer->tail);
    size_t space;

    PR_INFO("entry");

    space = CIRC_SPACE(head, tail, buffer->capacity);
    if (space >= 1) {
        memcpy(buffer->buffer + head*(buffer->element_size), element, buffer->element_size);
        WRITE_ONCE(buffer->head, (head + 1) & (buffer->capacity - 1));
        // TODO investigate smp
        // smp_store_release(buffer->head, (head + 1) & (buffer->size - 1));
        head = READ_ONCE(buffer->head);
        PR_INFO("capacity : %ld, head : %d, tail : %d", buffer->capacity, head, tail);
        // PR_INFO("capacity : %ld, buffer->head : %d, buffer->tail : %d", buffer->capacity, buffer->head, buffer->tail);
    } else {
        retval = -1;
        PR_ERR("buffer is full, capacity : %ld, head : %d, tail : %d", buffer->capacity, head, tail);
    }

    return retval;
}

void* front(struct CircularBufferPow2* buffer)
{
    void* event = NULL;
    int tail = READ_ONCE(buffer->tail);

    PR_INFO("entry");

    if (CIRC_CNT(buffer->head, tail, buffer->capacity)) {
        event = buffer->buffer + (tail)*(buffer->element_size);
        PR_INFO("capacity : %ld, head : %d, tail : %d", buffer->capacity, buffer->head, tail);
    }

    return event;
}

void pop(struct CircularBufferPow2* buffer)
{
    int tail = READ_ONCE(buffer->tail);

    PR_INFO("entry");

    if (CIRC_CNT(buffer->head, tail, buffer->capacity)) {
        WRITE_ONCE(buffer->tail, (tail + 1) & (buffer->capacity - 1));
        tail = READ_ONCE(buffer->tail);
        PR_INFO("capacity : %ld, head : %d, tail : %d", buffer->capacity, buffer->head, tail);
    } else {
        PR_ERR("buffer is empty");
    }
}

