#pragma once

/* max number of struct element_type (mod 2) in a PAGE_SIZE */
#define MAX_MOD_2_BUFFER_ELEMENTS(element_type) ( (PAGE_SIZE / sizeof(struct element_type)) - (PAGE_SIZE / sizeof(struct element_type)) % 2 )

/* circular buffer with mod 2 num_elements */
struct CircularBufferMod2 {
    void* buffer;
    size_t element_size;            /* selement size */
    size_t capacity;                /* num of elements allocated */
    int64_t size;                   /* num elements */
    int64_t peak;                   /* peak element usage */
    // int64_t head;
    // int64_t tail;
    int head;
    int tail;
};

int init(struct CircularBufferMod2* circular_buffer_mod2, size_t element_size, size_t num_elements);
void release(struct CircularBufferMod2* buffer);

bool empty(struct CircularBufferMod2* buffer);
void clear(struct CircularBufferMod2* buffer);
size_t size(struct CircularBufferMod2* buffer);
size_t space(struct CircularBufferMod2* buffer);
size_t events_pending_to_end(struct CircularBufferMod2* buffer);
int push(struct CircularBufferMod2* buffer, void* element);
void* front(struct CircularBufferMod2* buffer);
void pop(struct CircularBufferMod2* buffer);
