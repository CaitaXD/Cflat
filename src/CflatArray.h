#ifndef CFLAT_ARRAY_H
#define CFLAT_ARRAY_H

#include "CflatCore.h"

#define cflat_array_fields(T)  \
    usize length;               \
    T data[]                    \

#define cflat_define_array(T, ...) struct __VA_ARGS__ {         \
    cflat_array_fields(T);                                      \
}

#define cflat_assert_array_type(TArray)     \
    cflat_assert_type_member(TArray, length),    \
    cflat_assert_type_member(TArray, data[0])    \

#define cflat_assert_array_ptr(arr)                 \
    cflat_assert_ptr_has_member((arr), length),     \
    cflat_assert_ptr_has_member((arr), data[0])     \

typedef cflat_define_array(byte) CflatOpaqueArray;

#define cflat_array_allocation_size(T, len) (sizeof(CflatOpaqueArray) + (len) * sizeof(T))

typedef struct cflat_array_alloc_opt {
    usize align;
    bool clear;
} CflatArrayAllocOpt;

CFLAT_DEF CflatOpaqueArray* cflat_array_new_opt(usize element_size, usize length, CflatArena *a, CflatArrayAllocOpt opt);

#define cflat_array_new(TArray, size, arena, ...) (                                                                             \
    cflat_assert_array_type(TArray),                                                                                            \
    (TArray)cflat_array_new_opt(cflat_sizeof_member(TArray, data[0]), (size), (arena), ((CflatArrayAllocOpt) {__VA_ARGS__}))    \
)

CFLAT_DEF CflatOpaqueArray* (cflat_array_init)(usize element_size, void* memory, usize length, bool clear);

#define cflat_array_init(arr, len, clear) (                                             \
    cflat_assert_array_ptr(arr),                                                        \
    (cflat_typeof(arr))(cflat_array_init)(sizeof (arr)->data[0], (arr), (len), (clear)) \
)

#if defined(CFLAT_ARENA_IMPLEMENTATION)

CflatOpaqueArray* cflat_array_new_opt(const usize element_size, const usize length, CflatArena *a, CflatArrayAllocOpt opt) {
    const usize byte_size = length * element_size + sizeof(CflatOpaqueArray);
    CflatOpaqueArray *arr = cflat_arena_push_opt(a, byte_size, (CflatAllocOpt) {
        .align = opt.align,
        .clear = opt.clear,
    });
    arr->length = length;
    return arr;
}

static CflatOpaqueArray *const cflat_array_header_empty = &(CflatOpaqueArray){0};

CflatOpaqueArray* (cflat_array_init)(const usize element_size, void* memory, const usize length, const bool clear) {
    if (memory == NULL) {
        return cflat_array_header_empty;
    }
    CflatOpaqueArray *array = memory;
    array->length = length;
    if (clear) cflat_mem_zero(array->data, array->length*element_size);
    return array;
}

#endif // CFLAT_ARENA_IMPLEMENTATION


#ifndef CFLAT_ARRAY_NO_ALIAS
#   define array_allocation_size cflat_array_allocation_size
#   define array_new cflat_array_new
#   define array_init cflat_array_init
#   define define_array cflat_define_array
#   define define_array cflat_define_array
#   define array_fields cflat_array_fields
#endif //CFLAT_ARRAY_NO_ALIAS

#endif //CFLAT_ARRAY_H