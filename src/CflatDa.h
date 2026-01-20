#ifndef CFLAT_DA_H
#define CFLAT_DA_H

#include "CflatArena.h"
#include "CflatCore.h"

#define cflat_da_fields(T)  \
    usize capacity;         \
    usize count;            \
    T data[]                \

#define cflat_define_da(T, ...) struct __VA_ARGS__ {        \
    cflat_da_fields(T);                                     \
}

typedef cflat_define_da(byte, cflat_opaque_da) CflatOpaqueDa;

typedef struct cflat_da_new_opt {
    usize capacity;
    usize align;
    bool clear;
} CflatDaNewOpt;

CFLAT_DEF CflatOpaqueDa* cflat_da_new_opt(usize element_size, CflatArena *a, CflatDaNewOpt opt);

#define cflat_da_new(TDa, Arena, ...)\
    (TDa*)cflat_da_new_opt(cflat_sizeof_member(TDa, data[0]), (Arena), OVERRIDE_INIT(CflatDaNewOpt, __VA_ARGS__))

typedef struct cflat_da_init_opt {
    usize capacity;
    bool clear;
} CflatDaInitOpt;

CFLAT_DEF CflatOpaqueDa* cflat_da_init_opt(usize element_size, void* memory, usize capacity, CflatDaInitOpt opt);

#define cflat_da_init(TDa, da, cap, ...)\
    (TDa*)cflat_da_init_opt(cflat_sizeof_member(TDa, data[0]), (da), (cap), OVERRIDE_INIT(CflatDaNewOpt, __VA_ARGS__))

#define cflat_da_lit(TDa, cap)\
    cflat_padded_struct(TDa, (cap)*cflat_sizeof_member(TDa, data[0]), .capacity=(cap), .count = 0)

CFLAT_DEF CflatOpaqueDa* cflat_da_clone_opt(usize element_size, CflatArena *a, const CflatOpaqueDa *da, CflatDaNewOpt opt);

#define cflat_da_clone(a, da, ...)\
    (cflat_typeof(da))cflat_da_clone_opt(sizeof(*(da)->data), (a), (CflatOpaqueDa*)(da), OVERRIDE_INIT(CflatDaNewOpt, .align = cflat_alignof(*(da)->data), __VA_ARGS__))

#define cflat_da_at(da, index) (cflat_bounds_check((index), (da)->count), &(da)->data[(index)])
#define cflat_da_get(da, index) (*(cflat_da_at((da), (index))))

typedef struct cflat_da_ensure_capacity_opt {
    usize align;
    bool clear;
} CflatDaEnsureCapacityOpt;

CFLAT_DEF CflatOpaqueDa* cflat_da_ensure_capacity_opt(usize element_size, CflatArena *a, void *da, usize capacity, CflatDaEnsureCapacityOpt opt);

#define cflat_da_ensure_capacity(da, capacity, ...)\
    (cflat_typeof(da))cflat_da_ensure_capacity_opt(sizeof(*(da)->data), (da), (da), (capacity), OVERRIDE_INIT(CflatDaEnsureCapacityOpt, .align = cflat_alignof(*(da)->data), .clear = false, __VA_ARGS__))

#define cflat_arena_da_append(arena, da, value)                                                                                 \
    do {                                                                                                                        \
        CflatDaEnsureCapacityOpt opt = (CflatDaEnsureCapacityOpt) { .align = cflat_alignof(*(da)->data), .clear = false };      \
        (da) = (void*)cflat_da_ensure_capacity_opt(sizeof *(da)->data, (arena), (da), (da)->count + 1, opt);                    \
        (da)->data[(da)->count++] = (value);                                                                                    \
    } while (0)

#if defined(CFLAT_IMPLEMENTATION)

CflatOpaqueDa* cflat_da_new_opt(const usize element_size, CflatArena *a, CflatDaNewOpt opt) {
    if (opt.capacity == 0) opt.capacity = 4;

    const usize byte_size = sizeof(CflatOpaqueDa) + opt.capacity * element_size;

    CflatOpaqueDa *da = cflat_arena_push_opt(a, byte_size, (CflatAllocOpt) {
            .align = opt.align,
            .clear = opt.clear,
    });

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

CflatOpaqueDa* cflat_da_ensure_capacity_opt(usize element_size, CflatArena *a, void *da, usize capacity, CflatDaEnsureCapacityOpt opt) {
    CflatOpaqueDa *p = da;
    if (p == NULL) {
        p = cflat_da_new_opt(element_size, a,                   (CflatDaNewOpt) { .capacity = capacity,             .align = opt.align, .clear = opt.clear });
    } else if ((uptr)((uptr)a->curr + a->curr->pos) == (uptr)(p->data + element_size*p->capacity)) {
        cflat_arena_push_opt(a, element_size*p->capacity,       (CflatAllocOpt) {                                   .align = 1,         .clear = opt.clear });
    } else if (p->capacity < capacity) {
        p = cflat_da_clone_opt(element_size, a, p,              (CflatDaNewOpt) { .capacity = next_pow2(capacity),  .align = opt.align, .clear = opt.clear });
    }
    return p;
}

#endif // CFLAT_IMPLEMENTATION

#ifndef CFLAT_DA_NO_ALIAS
#   define arena_da_append cflat_arena_da_append
#   define da_ensure_capacity cflat_da_ensure_capacity
#   define da_new cflat_da_new
#   define da_clone cflat_da_clone
#   define da_fields cflat_da_fields
#   define define_da cflat_define_da
#   define da_lit cflat_da_lit
#   define da_at cflat_da_at
#   define da_get cflat_da_get
#endif // CFLAT_DA_NO_ALIAS

#endif //CFLAT_DA_H
