#ifndef CFLAT_DA_H
#define CFLAT_DA_H

#include "CflatArena.h"
#include "CflatCore.h"

#define cflat_da_fields(T)  \
    CflatArena *arena;      \
    usize count;            \
    usize capacity;         \
    T data[]                \

#define cflat_define_da(T, ...) struct __VA_ARGS__ {        \
    cflat_da_fields(T);                                     \
}

typedef cflat_define_da(byte) CflatOpaqueDa;

typedef struct cflat_da_new_opt {
    usize capacity;
    usize align;
    bool clear;
} CflatDaNewOpt;

typedef struct cflat_da_init_opt {
    usize capacity;
    bool clear;
} CflatDaInitOpt;

CFLAT_DEF CflatOpaqueDa* cflat_da_new_opt(usize element_size, CflatArena *a, CflatDaNewOpt opt);
CFLAT_DEF CflatOpaqueDa* cflat_da_init_opt(usize element_size, void* memory, usize capacity, CflatDaInitOpt opt);

#define cflat_da_new(TDa, Arena, ...)   (                                                                                           \
    (TDa*)cflat_da_new_opt(cflat_sizeof_member(TDa, data[0]), (Arena), (CflatDaNewOpt) {__VA_ARGS__})                               \
)

#define cflat_da_init(TDa, da, cap, ...) (                                                                                          \
    (TDa*)cflat_da_init_opt(cflat_sizeof_member(TDa, data[0]), (da), (cap), ((CflatDaNewOpt){__VA_ARGS__}))                         \
)

#define cflat_da_lit(TDa, cap) (                                                                                                    \
    cflat_padded_struct(TDa, (cap)*cflat_sizeof_member(TDa, data[0]), .arena=NULL, .capacity=(cap), .count=0)                       \
)

CFLAT_DEF CflatOpaqueDa* cflat_da_clone_opt(usize element_size, CflatArena *a, const CflatOpaqueDa *da, CflatDaNewOpt opt);

#define cflat_da_clone(a, da, ...) (                                                                             \
    (void*)cflat_da_clone_opt(sizeof(*(da)->data), (a), (CflatOpaqueDa*)(da), (CflatDaNewOpt) {__VA_ARGS__})     \
)

#define cflat_da_append(da, value)                                                                  \
    do {                                                                                            \
        CflatOpaqueDa *_da = (void*)(da);                                                           \
        cflat_assert(_da != NULL);                                                                  \
        const usize _esize = sizeof (da)->data[0];                                                  \
        if (_da->count >= _da->capacity) {                                                          \
            cflat_assert(_da->arena != NULL && "Da was fixed size");                                \
            _da = cflat_da_clone_opt(_esize, _da->arena, _da, (CflatDaNewOpt) {                     \
                .capacity=cflat_max(_da->capacity*2,4),                                             \
                .align=_esize,                                                                      \
                .clear=false                                                                        \
            });                                                                                     \
            (da) = (void*)_da;                                                                      \
        }                                                                                           \
                                                                                                    \
        (da)->data[_da->count++] = (value);                                                         \
    } while (0)

#if defined(CFLAT_IMPLEMENTATION)

CflatOpaqueDa* cflat_da_new_opt(const usize element_size, CflatArena *a, CflatDaNewOpt opt) {
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

CflatOpaqueDa* cflat_da_init_opt(usize element_size, void* memory, usize capacity, CflatDaInitOpt opt) {
    CflatOpaqueDa*da= memory;
    da->capacity = capacity;
    if (opt.clear) cflat_mem_zero(da->data, da->capacity*element_size);
    return da;
}

CflatOpaqueDa* cflat_da_clone_opt(const usize element_size, CflatArena *a, const CflatOpaqueDa *da, CflatDaNewOpt opt) {
    if (da == NULL) return NULL;

    opt.capacity = cflat_max(da->capacity, opt.capacity);
    CflatOpaqueDa *new_da = cflat_da_new_opt(element_size, a, opt);
    new_da->count = da->count;
    cflat_mem_copy(new_da->data, da->data, da->count*element_size);
    return new_da;
}

#endif // CFLAT_IMPLEMENTATION

#ifndef CFLAT_DA_NO_ALIAS
#   define da_append cflat_da_append
#   define da_new cflat_da_new
#   define da_clone cflat_da_clone
#   define da_fields cflat_da_fields
#   define define_da cflat_define_da
#   define da_lit cflat_da_lit
#endif // CFLAT_DA_NO_ALIAS

#endif //CFLAT_DA_H
