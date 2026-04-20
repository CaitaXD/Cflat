#ifndef CFLAT_DA_H
#define CFLAT_DA_H

#include "CflatArena.h"
#include "CflatBit.h"
#include "CflatCore.h"
#include "CflatSlice.h"
#include <stdio.h>

#define cflat_slice_resize(ARENA, DA, HINT)                                                                                      \
    do {                                                                                                                         \
        const usize element_size = sizeof(*(DA)->data);                                                                          \
        const usize element_alignment = cflat_alignof(max_align_t);                                                              \
        const CflatAllocOpt alloc_opt = (CflatAllocOpt) { .align = element_alignment, .clear = false };                          \
        if ((HINT) > (DA)->capacity) {                                                                                           \
            const usize old_size = (DA)->capacity * element_size;                                                                \
            if ((DA)->capacity == 0) (DA)->capacity = 4;                                                                         \
            while ((HINT) > (DA)->capacity) (DA)->capacity *= 2;                                                                 \
            const usize new_size = cflat_next_pow2_u64((DA)->capacity * element_size);                                           \
            (DA)->data = cflat_arena_extend_opt((ARENA), (DA)->data, old_size, new_size , alloc_opt);                            \
        }                                                                                                                        \
    } while (0)

#define cflat_slice_append(ARENA, DA, VAL)                                                                                       \
    do {                                                                                                                         \
        cflat_slice_resize((ARENA), (DA), (DA)->length + 1);                                                                     \
        (DA)->data[(DA)->length++] = (VAL);                                                                                      \
    } while (0)

#define cflat_slice_append_fixed(DA, VAL)                                                                                        \
    do {                                                                                                                         \
        (void)cflat_bounds_check((DA)->length, (DA)->capacity);                                                                  \
        (DA)->data[(DA)->length++] = (VAL);                                                                                      \
    } while (0)

#define cflat_slice_remove_unordered(DA, INDEX)                                                                                  \
    do {                                                                                                                         \
        *cflat_slice_at(*(DA), (INDEX)) = (DA)->data[--(DA)->length];                                                            \
    } while(0)

#define cflat_slice_remove(SLICE, INDEX)                                                                                         \
    do {                                                                                                                         \
        if ((usize)(INDEX) < (SLICE)->length) {                                                                                  \
            cflat_mem_move(&(SLICE)->data[(INDEX)], &(SLICE)->data[(INDEX) + 1],                                                 \
                    ((SLICE)->length - (INDEX) - 1) * sizeof(*(SLICE)->data));                                                   \
            (SLICE)->length--;                                                                                                   \
        }                                                                                                                        \
    } while(0)

#define cflat_slice_insert(ARENA, SLICE, INDEX, VAL)                                                                             \
    do {                                                                                                                         \
            cflat_slice_resize((ARENA), (SLICE), (SLICE)->length + 1);                                                           \
            cflat_mem_move(&(SLICE)->data[(INDEX) + 1], &(SLICE)->data[(INDEX)],                                                 \
                    ((SLICE)->length - (INDEX)) * sizeof(*(SLICE)->data));                                                       \
            (SLICE)->data[(INDEX)] = (VAL);                                                                                      \
            (SLICE)->length++;                                                                                                   \
    } while(0)

#define cflat_slice_insert_fixed(SLICE, INDEX, VAL)                                                                              \
    do {                                                                                                                         \
            (void)cflat_bounds_check((SLICE)->length, (SLICE)->capacity);                                                        \
            cflat_mem_move(&(SLICE)->data[(INDEX) + 1], &(SLICE)->data[(INDEX)],                                                 \
                    ((SLICE)->length - (INDEX)) * sizeof(*(SLICE)->data));                                                       \
            (SLICE)->data[(INDEX)] = (VAL);                                                                                      \
            (SLICE)->length++;                                                                                                   \
    } while(0)

#ifndef CFLAT_DA_NO_ALIAS
#   define slice_resize cflat_slice_resize
#   define slice_append cflat_slice_append
#   define slice_remove cflat_slice_remove
#   define slice_insert cflat_slice_insert
#   define slice_append_fixed cflat_slice_append_fixed
#   define slice_insert_fixed cflat_slice_insert_fixed
#endif // CFLAT_DA_NO_ALIAS

#endif //CFLAT_DA_H
