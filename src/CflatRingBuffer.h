#ifndef CFLAT_RING_BUFFER_H
#define CFLAT_RING_BUFFER_H

#include "CflatCore.h"
#include "CflatArena.h"

typedef struct opaque_ring_buffer {
    isize read;
    isize write;
    usize mask;
    byte data[];
} OpaqueRingBuffer;

typedef struct ring_buffer_new_opt {
    usize align;
    bool clear;
} RingBufferNewOpt;

OpaqueRingBuffer *ring_buffer_new_opt(usize element_size, Arena *a, usize length, RingBufferNewOpt opt);

bool (ring_buffer_is_empty)(OpaqueRingBuffer *rb);

usize (ring_buffer_count)(OpaqueRingBuffer *rb);

bool (ring_buffer_write)(usize element_size, OpaqueRingBuffer *rb, const void *src);

void (ring_buffer_overwrite)(usize element_size, OpaqueRingBuffer *rb, const void *src);

bool (ring_buffer_read)(usize element_size, OpaqueRingBuffer *rb, void *dst);

void (ring_buffer_clear)(OpaqueRingBuffer *rb);

#if defined(CFLAT_IMPLEMENTATION)

OpaqueRingBuffer *ring_buffer_new_opt(usize element_size, Arena *a, usize length, RingBufferNewOpt opt) {
    
    OpaqueRingBuffer *rb = cflat_arena_push_opt(a, length * element_size + sizeof(OpaqueRingBuffer), (CflatAllocOpt) {
        .align = opt.align,
        .clear = opt.clear,
    });

    rb->read = rb->write = 0;
    rb->mask = next_pow2(length) - 1;
    return rb;
}

bool (ring_buffer_is_empty)(OpaqueRingBuffer *rb) {
    
    if (rb == NULL) return true;
    
    usize read = rb->read;
    usize write = rb->write;

    return read == write;
}

bool (ring_buffer_read)(usize element_size, OpaqueRingBuffer *rb, void *dst) {
    
    if (rb == NULL) return false;
    
    usize read = rb->read;
    usize write = rb->write;
    
    if (read == write) return false;
    
    const void *src = rb->data + read*element_size;
    if (dst) memcpy(dst, src, element_size);
    
    read = (read + 1) & rb->mask;
    rb->read = read;
    return true;
}

isize (ring_buffer_read_buffer)(usize element_size, OpaqueRingBuffer *rb, void *dst, isize count) {
    if (rb == NULL) return 0;
    
    isize written = 0;
    while((written < count) && ring_buffer_read(element_size, rb, dst)) {
        written += 1;
        if (dst) dst += element_size;
    }
    
    return written;
}

bool (ring_buffer_write)(usize element_size, OpaqueRingBuffer *rb, const void *src) {
    
    if (rb == NULL) return false;

    usize count = rb->write - rb->read;
    usize capacity = rb->mask + 1;
    if (count == capacity) {
        return false;
    }
    
    ring_buffer_write(element_size, rb, src);
    return true;
}

void (ring_buffer_overwrite)(usize element_size, OpaqueRingBuffer *rb, const void *src) {
    
    if (rb == NULL) return;

    usize read = rb->read;
    usize write = rb->write;
    
    if(src) memcpy(rb->data + write*element_size, src, element_size);
    
    usize next_write = (write + 1) & rb->mask;
    
    if (read == next_write) {
        read = (read + 1) & rb->mask;
    }

    rb->write = next_write;
    rb->read = read;
}

void (ring_buffer_clear)(OpaqueRingBuffer *rb) {
    rb->read = rb->write = 0;
}

usize (ring_buffer_count)(OpaqueRingBuffer *rb) {
    
    if (rb == NULL) return 0;
    
    isize read = rb->read;
    isize write = rb->write;
    
    if (write >= read) return write - read;
    
    usize length = rb->mask + 1;
    return (length - read) + write;
}

#endif // CFLAT_IMPLEMENTATION

#endif //CFLAT_RING_BUFFER_H