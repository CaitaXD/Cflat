#ifndef CFLAT_DA_H
#define CFLAT_DA_H

#include "CflatArena.h"

#define cflat_da_fields(T)  \
    CflatArena *arena;      \
    usize count;            \
    usize capacity;         \
    T data[]                \

#define cflat_assert_da_type(TDa)       \
    cflat_assert_type_member(TDa, arena),    \
    cflat_assert_type_member(TDa, capacity), \
    cflat_assert_type_member(TDa, count),    \
    cflat_assert_type_member(TDa, data[0])   \

#define cflat_assert_da_ptr(da)                     \
    cflat_assert_ptr_has_member((da), arena),       \
    cflat_assert_ptr_has_member((da), capacity),    \
    cflat_assert_ptr_has_member((da), count),       \
    cflat_assert_ptr_has_member((da), data[0]),     \

#define cflat_define_da(T, ...) struct __VA_ARGS__ {        \
    cflat_da_fields(T);                                     \
}

typedef cflat_define_da(byte) CflatOpaqueDa;

typedef struct cflat_da_alloc_opt {
    usize capacity;
    usize align;
    bool clear;
} CflatDaAllocOpt;

CFLAT_DEF CflatOpaqueDa* cflat_da_new_opt(usize element_size, CflatArena *a, CflatDaAllocOpt opt);
#define cflat_da_new(TDa, Arena, ...)   (                                                                   \
    cflat_assert_da_type(TDa),                                                                              \
    (TDa)cflat_da_new_opt(cflat_sizeof_member(TDa, data[0]), (Arena), (CflatDaAllocOpt) {__VA_ARGS__})      \
)

CFLAT_DEF CflatOpaqueDa* cflat_da_clone_opt(usize element_size, const CflatOpaqueDa *da, CflatDaAllocOpt opt);
#define cflat_da_clone(da, ...) (                                                                           \
    cflat_assert_da_ptr(da)                                                                                 \
    (void*)cflat_da_clone_opt(sizeof(*(da)->data), (CflatOpaqueDa*)(da), (CflatDaAllocOpt) {__VA_ARGS__})   \
)

#define cflat_da_allocation_size(T, cap) (sizeof(CflatOpaqueDa) + (cap) * sizeof(T))

#define cflat_da_append(da, value)                                                                  \
    do {                                                                                            \
        CflatOpaqueDa *_da = (void*)(da);                                                           \
        cflat_assert(_da && "da should not be null");                                               \
        const usize _esize = sizeof (da)->data[0];                                                  \
                                                                                                    \
        if (_da->count >= _da->capacity) {                                                          \
            if (_da->capacity == 0) _da->capacity = 4;                                              \
            else _da->capacity *= 2;                                                                \
            _da = cflat_da_clone_opt(_esize, _da, (CflatDaAllocOpt) {                               \
                .capacity=_da->capacity,                                                            \
                .align=_esize,                                                                      \
                .clear=false                                                                        \
            });                                                                                     \
            (da) = (void*)_da;                                                                      \
            /*TODO:arena_realloc*/                                                                  \
        }                                                                                           \
                                                                                                    \
        (da)->data[_da->count++] = (value);                                                         \
    } while (0)

#if defined(CFLAT_DA_IMPLEMENTATION)

CflatOpaqueDa* cflat_da_new_opt(const usize element_size, CflatArena *a, CflatDaAllocOpt opt) {
    if (opt.capacity == 0) opt.capacity = 4;

    const usize byte_size = sizeof(CflatOpaqueDa) + opt.capacity * element_size;
    CflatOpaqueDa *da = cflat_arena_push_opt(a, byte_size, (CflatAllocOpt) {
        .align = opt.align,
        .clear = opt.clear,
    });
    da->arena = a;
    da->capacity = opt.capacity;
    da->count = 0;
    return da;
}

CflatOpaqueDa* cflat_da_clone_opt(const usize element_size, const CflatOpaqueDa *da, CflatDaAllocOpt opt) {
    if (da == NULL) return NULL;

    opt.capacity = cflat_max(da->capacity, opt.capacity);
    CflatOpaqueDa *new_da = cflat_da_new_opt(element_size, da->arena, opt);
    cflat_mem_copy(new_da, da, sizeof *da + da->count*element_size);
    return new_da;
}

#endif // CFLAT_DA_IMPLEMENTATION

#ifndef CFLAT_DA_NO_ALIAS
#   define da_allocation_size cflat_da_allocation_size
#   define da_append cflat_da_append
#   define da_new cflat_da_new
#   define da_clone cflat_da_clone
#   define da_fields cflat_da_fields
#   define define_da cflat_define_da
#endif // CFLAT_DA_NO_ALIAS

#endif //CFLAT_DA_H