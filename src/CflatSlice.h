#ifndef CFLAT_SLICE_H
#define CFLAT_SLICE_H

#include "CflatCore.h"

#define cflat_slice_fields(T)           \
    T* data;                            \
    usize length                        \

#define cflat_define_slice(T, ...) struct __VA_ARGS__ {     \
    cflat_slice_fields(T);                                  \
}

#define cflat_assert_slice_type(TSlice)     \
    cflat_assert_type_member(TSlice, length),    \
    cflat_assert_type_member(TSlice, data)       \

#define cflat_assert_slice(slice)                       \
    cflat_assert_ptr_has_member((slice), length),       \
    cflat_assert_ptr_has_member((slice), data)          \

typedef cflat_define_slice(void) CflatOpaqueSlice;

CFLAT_DEF CflatOpaqueSlice (cflat_slice_skiptake)(usize element_size, CflatOpaqueSlice slice, isize offset, isize length);

#define cflat_slice(TSlice, p, len) (           \
    cflat_assert_slice_type(TSlice),            \
    (TSlice) { .data = (p), .length = (len) }   \
)

#define cflat_slice_skiptake(slice, offset, len) (                          \
    cflat_assert_slice(slice),                                       \
    *(typeof((slice))*)                                                     \
    &cflat_lit((cflat_slice_skiptake)(sizeof (slice).data,                  \
        ((CflatOpaqueSlice){.data=(slice).data,.length=(slice).length}),    \
        (offset),                                                           \
        (len))                                                              \
    )                                                                       \
)

#define cflat_slice_skip(slice, skip) cflat_slice_skiptake((slice), (skip), (slice).length)
#define cflat_slice_take(slice, take) cflat_slice_skiptake(slice, 0, (take))
#define cflat_slice_lit(TSlice, CARRAY) ((TSlice) { .data = (CARRAY), .length = sizeof(CARRAY)/sizeof(*CARRAY) })

#if defined(CFLAT_SLICE_IMPLEMENTATION)

CflatOpaqueSlice (cflat_slice_skiptake)(const usize element_size,
    const CflatOpaqueSlice slice, const isize offset, const isize length) {
    usize u_offset = offset;
    usize u_len = length;

    if (offset < 0) u_offset = slice.length + offset;
    if (length < 0) u_len = slice.length + length;

    return (CflatOpaqueSlice) {
        .data = (byte*)slice.data + u_offset*element_size,
        .length = u_len - u_offset
    };
}

#ifndef CFLAT_SLICE_NO_ALIAS

#   define slice cflat_slice
#   define slice_skiptake cflat_slice_skiptake
#   define slice_skip cflat_slice_skip
#   define slice_take cflat_slice_take
#   define define_slice cflat_define_slice
#   define slice_lit cflat_slice_lit

#endif // CFLAT_SLICE_NO_ALIAS



#endif // CFLAT_SLICE_IMPLEMENTATION

#endif //CFLAT_SLICE_H