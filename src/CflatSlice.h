#ifndef CFLAT_SLICE_H
#define CFLAT_SLICE_H

#include "CflatBit.h"
#include "CflatCore.h"
#include "CflatArena.h"
#include <corecrt.h>

typedef struct cflat_slice_struct {
    usize capacity;
    usize length;
    void *data;
} CflatSliceStruct;

typedef struct cflat_slice_new_opt {
    usize capacity;
    usize align;
    bool clear;
} CflatSliceNewOpt;

#define cflat_slice(T) cflat_typeof( T ( *[3] ) (CflatSliceStruct) )

#define cflat_slice_element_type(SLICE)    cflat_typeof( (* (SLICE) ) ( (CflatSliceStruct){0} ) )

#define cflat_slice_member(SLICE, MEMBER)  ( (CflatSliceStruct*) (SLICE) )->MEMBER

#define cflat_slice_element_size(SLICE)    sizeof( cflat_slice_element_type(SLICE) )
#define cflat_slice_element_align(SLICE)   cflat_alignof( cflat_slice_element_type(SLICE) )

#define cflat_slice_lit(PTR, LEN)     ( (cflat_slice( cflat_typeof( (*PTR) ) ) *)  &(CflatSliceStruct) { .data = (PTR), .length = (LEN) }  )
#define cflat_slice_data(SLICE)       ( (cflat_slice_element_type(SLICE) *)  cflat_slice_member( (SLICE), data)                            )
#define cflat_slice_length(SLICE)     (cflat_slice_member( (SLICE), length)                                                                )
#define cflat_slice_capacity(SLICE)   (cflat_slice_member( (SLICE), capacity)                                                              )
#define cflat_slice_at(SLICE, INDEX)  (cflat_bounds_check( (INDEX), cflat_slice_length( (SLICE) ) ), cflat_slice_data( (SLICE) ) + (INDEX) )

CFLAT_DEF CflatSliceStruct cflat__slice_new_opt(usize element_size, CflatArena *a, usize length, CflatSliceNewOpt opt);
CFLAT_DEF CflatSliceStruct cflat__subslice(usize element_size, const CflatSliceStruct *s, isize offset, isize length);

#define cflat_slice_new(T, A, LEN, ...)      ( *(cflat_slice (T)    *)                                                                                            \
    (CflatSliceStruct[1]) { cflat__slice_new_opt(sizeof(T), (A), (LEN), OVERRIDE_INIT(CflatSliceNewOpt, .capacity = 4, .align = cflat_alignof(T), __VA_ARGS__)) } \
)

#define cflat_subslice(SLICE, OFFSET, LEN)   ( *(cflat_typeof(SLICE)*)\
    (CflatSliceStruct[1]) { cflat__subslice(cflat_slice_element_size(SLICE), (void*)(SLICE), (OFFSET), (LEN))                                                   } \
) 

#define cflat_slice_skip(SLICE, SKIP) cflat_subslice((SLICE), (SKIP), (SLICE).length - (SKIP))
#define cflat_slice_take(SLICE, TAKE) cflat_subslice((SLICE), 0, (TAKE))

#if defined(CFLAT_IMPLEMENTATION)

CflatSliceStruct cflat__subslice(const usize element_size, const CflatSliceStruct *s, const isize offset, const isize length) {
    usize u_offset = offset;
    usize u_len = length;

    if (offset < 0) u_offset = s->length + offset;
    if (length < 0) u_len = s->length + length;

    CflatSliceStruct slice = {
        .data = (byte*)s->data + u_offset*element_size,
        .capacity = u_len,
        .length   = u_len,
    };
    return slice;
}

CflatSliceStruct cflat__slice_new_opt(usize element_size, CflatArena *a, usize length, CflatSliceNewOpt opt) {
    const usize hint     = cflat_max(length, opt.capacity);
    const usize capacity = cflat_next_pow2_u64(hint);
    const usize size     = capacity*element_size;
    CflatSliceStruct slice = {
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
#   define slice cflat_slice
#   define slice_length cflat_slice_length
#   define subslice cflat_subslice
#   define slice_skip cflat_slice_skip
#   define slice_take cflat_slice_take
#   define slice_at cflat_slice_at

#endif // CFLAT_SLICE_NO_ALIAS


#endif //CFLAT_SLICE_H
