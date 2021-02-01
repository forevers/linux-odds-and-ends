#pragma once

/* circular buffer with mod 2 num_elements */
struct CircularBufferPow2 {
    void* buffer;
    size_t element_size;            /* element size */
    size_t capacity;                /* num of elements allocated */
    int64_t size;                   /* num elements */
    int64_t peak;                   /* peak element usage */
    // int64_t head;
    // int64_t tail;
    int head;
    int tail;
};

int init(struct CircularBufferPow2* circular_buffer_mod2, size_t element_size, size_t num_elements);
void release(struct CircularBufferPow2* buffer);

bool empty(struct CircularBufferPow2* buffer);
void clear(struct CircularBufferPow2* buffer);
size_t size(struct CircularBufferPow2* buffer);
size_t space(struct CircularBufferPow2* buffer);
size_t events_pending_to_end(struct CircularBufferPow2* buffer);
int push(struct CircularBufferPow2* buffer, void* element);
void* front(struct CircularBufferPow2* buffer);
void pop(struct CircularBufferPow2* buffer);
