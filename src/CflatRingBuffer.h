#ifndef CFLAT_RING_BUFFER_H
#define CFLAT_RING_BUFFER_H

#include "CflatCore.h"
#include "CflatArena.h"
#include "CflatSlice.h"
#include <stdatomic.h>

typedef struct cflat_ring_buffer {
    _Atomic isize read;
    _Atomic isize write;
    usize length;
    byte data[];
} cflat_may_alias CflatRingBuffer;

typedef struct ring_buffer_new_opt {
    usize align;
    bool clear;
} RingBufferNewOpt;

typedef struct ring_buffer_read_opt {
    bool clear;
} RingBufferReadOpt;

CflatRingBuffer* cflat_ring_buffer_new_opt(usize element_size, Arena *a, usize length, RingBufferNewOpt opt);

usize cflat_ring_buffer_count             (CflatRingBuffer *rb                                                        );
bool cflat_ring_buffer_is_empty           (CflatRingBuffer *rb                                                        );
void cflat_ring_buffer_clear              (CflatRingBuffer *rb                                                        );
bool cflat_ring_buffer_write              (CflatRingBuffer *rb, usize element_size, const void *src                   );
bool cflat_ring_buffer_read_opt           (CflatRingBuffer *rb, usize element_size, void *dst, RingBufferReadOpt opt  );
void cflat_ring_buffer_overwrite          (CflatRingBuffer *rb, usize element_size, const void *src                   );

#define cflat_ring_buffer_read(rb, element_size, dst, ...) ring_buffer_read_opt(rb, element_size, dst, CFLAT_OPT(RingBufferReadOpt, .clear = false, __VA_ARGS__))

#if defined(CFLAT_IMPLEMENTATION)

CflatRingBuffer *cflat_ring_buffer_new_opt(usize element_size, Arena *a, usize length, RingBufferNewOpt opt) {
    
    usize real_length = next_pow2(length);

    CflatRingBuffer *rb = cflat_arena_push_opt(a, real_length * element_size + sizeof(CflatRingBuffer), (CflatAllocOpt) {
        .align = opt.align,
        .clear = opt.clear,
    });

    rb->read = rb->write = 0;
    rb->length = real_length;
    return rb;
}

bool cflat_ring_buffer_is_empty(CflatRingBuffer *rb) {
    
    if (rb == NULL) return true;
    
    usize read = rb->read;
    usize write = rb->write;

    return read == write;
}

bool cflat_ring_buffer_read_opt(CflatRingBuffer *rb, usize element_size, void *dst, RingBufferReadOpt opt) {
    
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

bool cflat_ring_buffer_write(CflatRingBuffer *rb, usize element_size, const void *src) {
    
    if (rb == NULL) return false;

    usize count = rb->write - rb->read;
    if (count == rb->length) {
        return false;
    }
    
    cflat_ring_buffer_overwrite(rb, element_size, src);
    return true;
}

void cflat_ring_buffer_overwrite(CflatRingBuffer *rb, usize element_size, const void *src) {
    
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

void cflat_ring_buffer_clear(CflatRingBuffer *rb) {
    rb->read = rb->write = 0;
}

usize cflat_ring_buffer_count(CflatRingBuffer *rb) {
    
    if (rb == NULL) return 0;
    
    isize read = rb->read;
    isize write = rb->write;
    
    if (write >= read) return write - read;
    
    return (rb->length - read) + write;
}

void cflat_ring_buffer_readable_chunks(CflatRingBuffer *rb, usize element_size, CflatByteSlice chunks[static 2]) {
    if (rb == NULL) {
        chunks[0] = (CflatByteSlice){0};
        chunks[1] = (CflatByteSlice){0};
        return;
    }
    
    usize read  = atomic_load_explicit(&rb->read, memory_order_acquire);
    usize write = atomic_load_explicit(&rb->write, memory_order_acquire);

    if (read <= write) {
        chunks[0] = (CflatByteSlice){ .data = rb->data + (read * element_size), .length = (write - read) * element_size };
        chunks[1] = (CflatByteSlice){0};
    } else {
        chunks[0] = (CflatByteSlice){ .data = rb->data + (read * element_size), .length = (rb->length - read) * element_size };
        chunks[1] = (CflatByteSlice){ .data = rb->data, .length = write * element_size };
    }
}

void cflat_ring_buffer_writable_chunks(CflatRingBuffer *rb, usize element_size, CflatByteSlice chunks[static 2]) {
    if (rb == NULL) {
        chunks[0] = (CflatByteSlice){0};
        chunks[1] = (CflatByteSlice){0};
        return;
    }
    
    usize read  = atomic_load_explicit(&rb->read, memory_order_acquire);
    usize write = atomic_load_explicit(&rb->write, memory_order_acquire);
    
    if (write >= read) {
        chunks[0] = (CflatByteSlice){ .data = rb->data + (write * element_size), .length = (rb->length - write) * element_size };
        chunks[1] = (CflatByteSlice){ .data = rb->data, .length = (read > 0 ? read - 1 : 0) * element_size };
        if (read == 0 && chunks[0].length > 0) {
            chunks[0].length -= element_size;
        }
    } else {
        chunks[0] = (CflatByteSlice){ .data = rb->data + (write * element_size), .length = (read - write - 1) * element_size };
        chunks[1] = (CflatByteSlice){0};
    }
}

#endif // CFLAT_IMPLEMENTATION

#if !defined(CFLAT_RING_BUFFER_NO_ALIAS)

#   define RingBuffer CflatRingBuffer
#   define ring_buffer_count cflat_ring_buffer_count
#   define ring_buffer_is_empty cflat_ring_buffer_is_empty
#   define ring_buffer_write cflat_ring_buffer_write
#   define ring_buffer_read cflat_ring_buffer_read
#   define ring_buffer_overwrite cflat_ring_buffer_overwrite
#   define ring_buffer_clear cflat_ring_buffer_clear
#   define ring_buffer_read_opt cflat_ring_buffer_read_opt
#   define ring_buffer_new_opt cflat_ring_buffer_new_opt

#endif // CFLAT_RING_BUFFER_NO_ALIAS

#endif //CFLAT_RING_BUFFER_H