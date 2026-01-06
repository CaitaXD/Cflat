#ifndef CFLAT_ARRAY_H
#define CFLAT_ARRAY_H

#include "CflatCore.h"
#include "CflatArena.h"

#define cflat_array_fields(T)                                                                                                       \
    usize length;                                                                                                                   \
    T data[]                                                                                                                        \

#define cflat_define_array(T, ...) struct __VA_ARGS__ {                                                                             \
    cflat_array_fields(T);                                                                                                          \
}

typedef cflat_define_array(byte) CflatOpaqueArray;

#define cflat_array_allocation_size(T, len) (sizeof(CflatOpaqueArray) + (len) * sizeof(T))

typedef struct cflat_array_new_opt {
    usize align;
    bool clear;
} CflatArrayNewOpt;

typedef struct cflat_array_init_opt {
    bool clear;
} CflatArrayInitOpt;

CFLAT_DEF CflatOpaqueArray* cflat_array_new_opt(usize element_size, usize length, CflatArena *a, CflatArrayNewOpt opt);
CFLAT_DEF CflatOpaqueArray* cflat_array_init_opt(usize element_size, void* memory, usize length, CflatArrayInitOpt opt);

#define cflat_array_new(TArray, size, arena, ...) (                                                                                 \
    (TArray*)cflat_array_new_opt(cflat_sizeof_member(TArray, data[0]), (size), (arena), ((CflatArrayNewOpt) {__VA_ARGS__}))         \
)

#define cflat_array_init(TArray, arr, len, ...) (                                                                                   \
    (TArray*)cflat_array_init_opt(cflat_sizeof_member(TArray, data[0]), (arr), (len), ((CflatArrayInitOpt){__VA_ARGS__}))           \
)

#define cflat_array_lit(TArray, len) (                                                                                              \
    cflat_padded_struct(TArray, (len)*cflat_sizeof_member(TArray, data[0]), .length=(len))                                          \
)


#if defined(CFLAT_IMPLEMENTATION)

CflatOpaqueArray* cflat_array_new_opt(const usize element_size, const usize length, CflatArena *a, CflatArrayNewOpt opt) {
    const usize byte_size = length * element_size + sizeof(CflatOpaqueArray);
    
    CflatOpaqueArray *arr = cflat_arena_push_opt(a, byte_size, (CflatAllocOpt) {
            .align = opt.align,
            .clear = opt.clear,
    });

    arr->length = length;
    return arr;
}

static CflatOpaqueArray *const cflat_array_empty = &(CflatOpaqueArray){0};

CflatOpaqueArray* cflat_array_init_opt(const usize element_size, void* memory, const usize length, CflatArrayInitOpt opt) {
    if (memory == NULL) {
        return cflat_array_empty;
    }
    CflatOpaqueArray *array = memory;
    array->length = length;
    if (opt.clear) cflat_mem_zero(array->data, array->length*element_size);
    return array;
}

#endif // CFLAT_IMPLEMENTATION


#ifndef CFLAT_ARRAY_NO_ALIAS
#   define array_allocation_size cflat_array_allocation_size
#   define array_new cflat_array_new
#   define array_init cflat_array_init
#   define define_array cflat_define_array
#   define define_array cflat_define_array
#   define array_fields cflat_array_fields
#   define array_lit cflat_array_lit
#endif //CFLAT_ARRAY_NO_ALIAS

#endif //CFLAT_ARRAY_H
