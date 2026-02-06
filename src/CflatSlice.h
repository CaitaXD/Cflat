#ifndef CFLAT_SLICE_H
#define CFLAT_SLICE_H

#include "CflatBit.h"
#include "CflatCore.h"
#include "CflatArena.h"

#define CFLAT_SLICE_FIELDS(T)    \
    usize capacity;              \
    usize length;                \
    T     *data                  \

typedef struct cflat_byte_slice CflatByteSlice;
typedef struct cflat_slice_new_opt CflatSliceNewOpt;

#define cflat_slice_new(TSlice, ARENA, LEN, ...) cflat_lvalue_cast(CflatByteSlice, TSlice) {                                                                                                    \
    cflat__slice_new_opt(cflat_sizeof_member(TSlice, data[0]), (ARENA), (LEN), OVERRIDE_INIT(CflatSliceNewOpt, .capacity = 4, .align = cflat_alignof_member(TSlice, data[0]), __VA_ARGS__))     \
}

#define cflat_subslice(SLICE, OFFSET, LEN) cflat_lvalue_cast(CflatByteSlice, cflat_typeof(SLICE)) {                                                                                             \
    cflat__subslice(sizeof((SLICE).data[0]), (void*)&(SLICE), (OFFSET), (LEN))                                                                                                                  \
}

#define cflat_slice_skip(SLICE, SKIP) cflat_subslice((SLICE), (SKIP), (SLICE).length - (SKIP))
#define cflat_slice_take(SLICE, TAKE) cflat_subslice((SLICE), 0, (TAKE))

#define cflat_slice_length(SLICE)     (SLICE).length
#define cflat_slice_capacity(SLICE)   (SLICE).capacity
#define cflat_slice_data(SLICE)       (SLICE).data
#define cflat_slice_at(SLICE, INDEX)  ( cflat_bounds_check( (INDEX), cflat_slice_length( (SLICE) ) ), \
                                        cflat_slice_data( (SLICE) ) + (INDEX)                         )

CFLAT_DEF CflatByteSlice cflat__slice_new_opt(usize element_size, CflatArena *a, usize length, CflatSliceNewOpt opt);
CFLAT_DEF CflatByteSlice cflat__subslice(usize element_size, const CflatByteSlice *s, isize offset, isize length);

#if defined(CFLAT_IMPLEMENTATION)

struct cflat_slice_new_opt {
    usize capacity;
    usize align;
    bool clear;
};

struct cflat_byte_slice { 
    CFLAT_SLICE_FIELDS(byte); 
};

CflatByteSlice cflat__subslice(const usize element_size, const CflatByteSlice *s, const isize offset, const isize length) {
    usize u_offset = offset;
    usize u_len = length;

    if (offset < 0) u_offset = s->length + offset;
    if (length < 0) u_len = s->length + length;

    CflatByteSlice slice = {
        .data = (byte*)s->data + u_offset*element_size,
        .capacity = u_len,
        .length   = u_len,
    };
    return slice;
}

CflatByteSlice cflat__slice_new_opt(usize element_size, CflatArena *a, usize length, CflatSliceNewOpt opt) {
    const usize hint     = cflat_max(length, opt.capacity);
    const usize capacity = cflat_next_pow2_u64(hint);
    const usize size     = capacity*element_size;
    CflatByteSlice slice = {
        .data = cflat_arena_push_opt(a, size, (CflatAllocOpt){opt.align, opt.clear}),
        .length = length,
        .capacity = capacity,
    };
    return slice;
}

#endif // CFLAT_IMPLEMENTATION

#ifndef CFLAT_SLICE_NO_ALIAS

#   define slice_data cflat_slice_data
#   define slice_new cflat_slice_new
#   define slice_length cflat_slice_length
#   define subslice cflat_subslice
#   define slice_skip cflat_slice_skip
#   define slice_take cflat_slice_take
#   define slice_at cflat_slice_at

#endif // CFLAT_SLICE_NO_ALIAS


#endif //CFLAT_SLICE_H
