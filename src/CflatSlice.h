#ifndef CFLAT_SLICE_H
#define CFLAT_SLICE_H

#include "CflatCore.h"
#include "CflatArena.h"

#define cflat_slice_fields(T)                                                                   \
    T* data;                                                                                    \
    usize length                                                                                \

#define cflat_define_slice(T, ...) struct __VA_ARGS__ {                                         \
    cflat_slice_fields(T);                                                                      \
}

typedef cflat_define_slice(void) CflatOpaqueSlice;

typedef struct cflat_slice_new_opt {
    usize align;
    bool clear;
} CflatSliceNewOpt;

CFLAT_DEF CflatOpaqueSlice cflat_slice_new_opt(usize element_size, usize length, CflatArena *a, CflatSliceNewOpt opt);

#define cflat_slice_new(TSlice, size, arena, ...) (                                                                             \
    *(TSlice*)cflat_slice_new_opt(cflat_sizeof_member(TSlice, data[0]), (size), (arena), ((CflatSliceNewOpt) {__VA_ARGS__}))    \
)

#define cflat_slice_init(TSlice, p, len) (                                                      \
    (TSlice) { .data = (p), .length = (len) }                                                   \
)

#define cflat_slice_lit(TSlice, CARRAY) (                                                       \
    (TSlice) { .data = (CARRAY), .length = cflat_array_length((CARRAY)) }                       \
)

CFLAT_DEF CflatOpaqueSlice (cflat_slice)(usize element_size, CflatOpaqueSlice slice, isize offset, isize length);

#define cflat_slice(slice, offset, len) (                                                       \
    *(cflat_typeof((slice))*) &cflat_lit((cflat_slice)(sizeof (slice).data,                     \
        ((CflatOpaqueSlice){.data=(slice).data,.length=(slice).length}),                        \
        (offset),                                                                               \
        (len))                                                                                  \
    )                                                                                           \
)

#define cflat_slice_skip(slice, skip) cflat_slice((slice), (skip), (slice).length-(skip))
#define cflat_slice_take(slice, take) cflat_slice(slice, 0, (take))

#if defined(CFLAT_IMPLEMENTATION)

CflatOpaqueSlice (cflat_slice)(const usize element_size,
    const CflatOpaqueSlice slice, const isize offset, const isize length) {
    usize u_offset = offset;
    usize u_len = length;

    if (offset < 0) u_offset = slice.length + offset;
    if (length < 0) u_len = slice.length + length;

    return (CflatOpaqueSlice) {
        .data = (byte*)slice.data + u_offset*element_size,
        .length = u_len
    };
}

CflatOpaqueSlice cflat_slice_new_opt(usize element_size, usize length, CflatArena *a, CflatSliceNewOpt opt) {
    
    const usize byte_size = length * element_size;
    void *data = cflat_arena_push_opt(a, byte_size, (CflatAllocOpt) {
            .align = opt.align,
            .clear = opt.clear,
    });

    return (CflatOpaqueSlice) {
        .data = data,
        .length = length
    };
}

#ifndef CFLAT_SLICE_NO_ALIAS

#   define slice_init cflat_slice_init
#   define slice cflat_slice
#   define slice_skip cflat_slice_skip
#   define slice_take cflat_slice_take
#   define define_slice cflat_define_slice
#   define slice_lit cflat_slice_lit
#   define slice_from_array cflat_slice_from_array

#endif // CFLAT_SLICE_NO_ALIAS



#endif // CFLAT_IMPLEMENTATION

#endif //CFLAT_SLICE_H
