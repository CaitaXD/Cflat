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
        CflatResizeOpt opt = (CflatResizeOpt) { .align = cflat_slice_element_align((da)), .clear = false };                                                         \
        CflatSliceStruct *impl = (void*)(da);                                                                                                                         \
        *impl = cflat__slice_resize_opt(cflat_slice_element_size((da)), (arena), *impl, impl->length + 1, opt);                                                     \
        cflat_slice_data((da))[impl->length++] = (value);                                                                                                           \
    } while (0)

#if defined(CFLAT_IMPLEMENTATION)

CflatSliceStruct cflat__slice_resize_opt(usize element_size, CflatArena *a, CflatSliceStruct s, usize capacity, CflatResizeOpt opt) {
    if (s.data == NULL) return cflat__slice_new_opt(element_size, a, 0, (CflatSliceNewOpt){capacity, opt.align, opt.clear});
    if (s.capacity >= capacity) return s;
    
    const usize old_size = element_size*s.capacity;
    const usize new_size = 2ULL*element_size*s.capacity;
    CflatSliceStruct ns = {
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
