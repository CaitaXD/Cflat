#ifndef CFLAT_DA_H
#define CFLAT_DA_H

#include "CflatArena.h"
#include "CflatBit.h"
#include "CflatCore.h"
#include "CflatSlice.h"
#include <stdio.h>

typedef struct cflat_da_init_opt {
    usize capacity;
    bool clear;
} CflatDaInitOpt;

typedef struct cflat_resize_opt {
    usize align;
    bool clear;
} CflatResizeOpt;

#define cflat_slice_append(arena, da, value)                                                                                                                        \
    do {                                                                                                                                                            \
        CflatResizeOpt resize_opt = (CflatResizeOpt) { .align = cflat_alignof(cflat_typeof((da).data[0])), .clear = false };                                        \
        CflatByteSlice *byte_slice = (void*)&(da);                                                                                                                  \
        *byte_slice = cflat__slice_resize_opt(sizeof((da).data[0]), (arena), *byte_slice, byte_slice->length + 1, resize_opt);                                      \
        (da).data[byte_slice->length++] = (value);                                                                                                                  \
    } while (0)

#if defined(CFLAT_IMPLEMENTATION)

CflatByteSlice cflat__slice_resize_opt(usize element_size, CflatArena *a, CflatByteSlice s, usize capacity, CflatResizeOpt opt) {
    if (s.data == NULL) return cflat__slice_new_opt(element_size, a, 0, (CflatSliceNewOpt){capacity, opt.align, opt.clear});
    if (s.capacity >= capacity) return s;
    
    const usize old_size = element_size*s.capacity;
    const usize new_size = 2ULL*element_size*s.capacity;
    CflatByteSlice ns = {
        .data = cflat_arena_extend_opt(a, s.data, old_size, new_size, (CflatAllocOpt){opt.align, opt.clear}),
        .length = s.length,
        .capacity = s.capacity * 2,
    };
    return ns;
}

#endif // CFLAT_IMPLEMENTATION

#ifndef CFLAT_DA_NO_ALIAS
#   define slice_append cflat_slice_append
#endif // CFLAT_DA_NO_ALIAS

#endif //CFLAT_DA_H
