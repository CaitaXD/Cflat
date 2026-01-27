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

typedef cflat_define_da(byte, cflat_dynamic_array) CflatDynamicArray;

typedef struct cflat_da_new_opt {
    usize capacity;
    usize align;
    bool clear;
} CflatDaNewOpt;

typedef struct cflat_da_init_opt {
    usize capacity;
    bool clear;
} CflatDaInitOpt;

typedef struct cflat_da_ensure_capacity_opt {
    usize align;
    bool clear;
} CflatDaEnsureCapacityOpt;

CFLAT_DEF CflatDynamicArray* cflat_da_new_opt            (usize element_size, CflatArena *a, CflatDaNewOpt opt                                                  );
CFLAT_DEF CflatDynamicArray* cflat_da_init_opt           (usize element_size, void* memory, usize capacity, CflatDaInitOpt opt                                  );
CFLAT_DEF CflatDynamicArray* cflat_da_clone_opt          (usize element_size, CflatArena *a, const CflatDynamicArray *da, CflatDaNewOpt opt                     );
CFLAT_DEF CflatDynamicArray* cflat_da_ensure_capacity_opt(usize element_size, CflatArena *a, CflatDynamicArray *da, usize capacity, CflatDaEnsureCapacityOpt opt);

#define cflat_da_new(TDa, Arena, ...)    (TDa*)cflat_da_new_opt(cflat_sizeof_member(TDa, data[0]), (Arena), OVERRIDE_INIT(CflatDaNewOpt, __VA_ARGS__))
#define cflat_da_init(TDa, da, cap, ...) (TDa*)cflat_da_init_opt(cflat_sizeof_member(TDa, data[0]), (da), (cap), OVERRIDE_INIT(CflatDaNewOpt, __VA_ARGS__))
#define cflat_da_lit(TDa, cap)           (cflat_padded_struct(TDa, (cap)*cflat_sizeof_member(TDa, data[0]), .capacity=(cap), .count = 0))
#define cflat_da_at(da, index)           (cflat_bounds_check((index), (da)->count), &(da)->data[(index)])
#define cflat_da_get(da, index)          (*(cflat_da_at((da), (index))))
#define cflat_da_count(da)               ((da) ? (da)->count : 0)

#define cflat_da_clone(A, DA, ...)           \
    (cflat_typeof(DA))                       \
    cflat_da_clone_opt(sizeof(*(DA)->data),  \
                       (A),                  \
                       (CflatDynamicArray*)(DA),   \
                       OVERRIDE_INIT(CflatDaNewOpt, .align = cflat_alignof(cflat_typeof(*(DA)->data)), __VA_ARGS__))

#define cflat_da_ensure_capacity(arena, da, capacity, ...) \
    (cflat_typeof(da))                                     \
    cflat_da_ensure_capacity_opt(sizeof(*(da)->data), \
                                 (arena),             \
                                 (void*)(da),         \
                                 (capacity),          \
                                 OVERRIDE_INIT(CflatDaEnsureCapacityOpt, .align = cflat_alignof(*(da)->data), .clear = false, __VA_ARGS__))

#define cflat_arena_da_append(arena, da, value)                                                                                         \
    do {                                                                                                                                \
        CflatDaEnsureCapacityOpt opt = (CflatDaEnsureCapacityOpt) { .align = cflat_alignof(cflat_typeof(*(da)->data)), .clear = false };\
        (da) = (void*)cflat_da_ensure_capacity_opt(sizeof(*(da)->data), (arena), (void*)(da), cflat_da_count((da)) + 1, opt);           \
        (da)->data[(da)->count++] = (value);                                                                                            \
    } while (0)

#if defined(CFLAT_IMPLEMENTATION)

CflatDynamicArray* cflat_da_new_opt(const usize element_size, CflatArena *a, CflatDaNewOpt opt) {

    opt.capacity = cflat_max(4, next_pow2(opt.capacity));

    const usize byte_size = sizeof(CflatDynamicArray) + opt.capacity * element_size;

    CflatDynamicArray *da = cflat_arena_push_opt(a, byte_size, (CflatAllocOpt) {
            .align = opt.align,
            .clear = opt.clear,
    });

    da->capacity = opt.capacity;
    da->count = 0;
    return da;
}

CflatDynamicArray* cflat_da_init_opt(usize element_size, void* memory, usize capacity, CflatDaInitOpt opt) {
    CflatDynamicArray*da= memory;
    da->capacity = capacity;
    if (opt.clear) cflat_mem_zero(da->data, da->capacity*element_size);
    return da;
}

CflatDynamicArray* cflat_da_clone_opt(const usize element_size, CflatArena *a, const CflatDynamicArray *da, CflatDaNewOpt opt) {
    if (da == NULL) return NULL;

    opt.capacity = cflat_max(da->capacity, opt.capacity);
    CflatDynamicArray *new_da = cflat_da_new_opt(element_size, a, opt);
    new_da->count = da->count;
    cflat_mem_copy(new_da->data, da->data, da->count*element_size);
    return new_da;
}

CflatDynamicArray* cflat_da_ensure_capacity_opt(usize element_size, CflatArena *a, CflatDynamicArray *da, usize capacity, CflatDaEnsureCapacityOpt opt) {
    
    if (da == NULL) return cflat_da_new_opt(element_size, a, (CflatDaNewOpt) { .align = opt.align, .capacity = capacity, .clear = opt.clear });
    if (da->capacity >= capacity) return da;
    
    CflatAllocOpt aopt = { .align = opt.align, .clear = opt.clear };
    CflatDynamicArray *new_da = cflat_arena_extend_opt(a, da, sizeof(*da) + element_size*da->capacity, sizeof(*da) + element_size*da->capacity*2, aopt);
    new_da->capacity *= 2;
    return new_da;
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
#   define da_count cflat_da_count
#   define DynamicArray CflatDynamicArray
#endif // CFLAT_DA_NO_ALIAS

#endif //CFLAT_DA_H
