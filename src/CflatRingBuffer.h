#ifndef CFLAT_RING_BUFFER_H
#define CFLAT_RING_BUFFER_H

#include "CflatCore.h"
#include "CflatArena.h"

typedef struct cflat_ring_buffer {
  isize read;
  isize write;
  usize length;
  byte data[];
} CflatRingBuffer;

typedef struct ring_buffer_new_opt {
    usize align;
    bool clear;
} RingBufferNewOpt;

typedef struct ring_buffer_read_opt {
    bool clear;
} RingBufferReadOpt;

CflatRingBuffer* ring_buffer_new_opt(usize element_size, Arena *a, usize length, RingBufferNewOpt opt);

usize ring_buffer_count(CflatRingBuffer *rb);
bool ring_buffer_is_empty(CflatRingBuffer *rb);
bool ring_buffer_write(usize element_size, CflatRingBuffer *rb, const void *src);
bool ring_buffer_read_opt(usize element_size, CflatRingBuffer *rb, void *dst, RingBufferReadOpt opt);
void ring_buffer_overwrite(usize element_size, CflatRingBuffer *rb, const void *src);
void ring_buffer_clear(CflatRingBuffer *rb);

#define ring_buffer_read(element_size, rb, dst, ...) ring_buffer_read_opt(element_size, rb, dst, CFLAT_OPT(RingBufferReadOpt, .clear = false, __VA_ARGS__))

#if defined(CFLAT_IMPLEMENTATION)

CflatRingBuffer *ring_buffer_new_opt(usize element_size, Arena *a, usize length, RingBufferNewOpt opt) {
    
    CflatRingBuffer *rb = cflat_arena_push_opt(a, length * element_size + sizeof(CflatRingBuffer), (CflatAllocOpt) {
        .align = opt.align,
        .clear = opt.clear,
    });

    rb->read = rb->write = 0;
    rb->length = next_pow2(length);
    return rb;
}

bool ring_buffer_is_empty(CflatRingBuffer *rb) {
    
    if (rb == NULL) return true;
    
    usize read = rb->read;
    usize write = rb->write;

    return read == write;
}

bool ring_buffer_read_opt(usize element_size, CflatRingBuffer *rb, void *dst, RingBufferReadOpt opt) {
    
    if (rb == NULL) return false;
    
    usize read = rb->read;
    usize write = rb->write;
    
    if (read == write) return false;
    
    void *src = rb->data + read*element_size;
    if (dst) memcpy(dst, src, element_size);
    if (opt.clear) mem_zero(src, element_size);
    
    read = (read + 1) % rb->length;
    rb->read = read;
    return true;
}

bool ring_buffer_write(usize element_size, CflatRingBuffer *rb, const void *src) {
    
    if (rb == NULL) return false;

    usize count = rb->write - rb->read;
    if (count == rb->length) {
        return false;
    }
    
    ring_buffer_overwrite(element_size, rb, src);
    return true;
}

void ring_buffer_overwrite(usize element_size, CflatRingBuffer *rb, const void *src) {
    
    if (rb == NULL) return;

    usize read = rb->read;
    usize write = rb->write;
    
    if(src) memcpy(rb->data + write*element_size, src, element_size);
    
    usize next_write = (write + 1) % rb->length;
    
    if (read == next_write) {
        read = (read + 1) % rb->length;
    }

    rb->write = next_write;
    rb->read = read;
}

void ring_buffer_clear(CflatRingBuffer *rb) {
    rb->read = rb->write = 0;
}

usize ring_buffer_count(CflatRingBuffer *rb) {
    
    if (rb == NULL) return 0;
    
    isize read = rb->read;
    isize write = rb->write;
    
    if (write >= read) return write - read;
    
    return (rb->length - read) + write;
}

#endif // CFLAT_IMPLEMENTATION

#endif //CFLAT_RING_BUFFER_H