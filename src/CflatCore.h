#ifndef CFLAT_DEF_H
#define CFLAT_DEF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef CFLAT_DEF
#   define CFLAT_DEF
#endif

#if defined(__unix__)
#  define OS_UNIX 1
#endif

#if __STDC_VERSION__ >= 202311L
#   define C23_OR_GREATER 1
#endif
#if __STDC_VERSION__ >= 201112L
#   define C11_OR_GREATER 1
#endif
#if __STDC_VERSION__ >= 199901L
#   define C99_OR_GREATER 1
#endif

#if defined(_WIN32)
#  define OS_WINDOWS 1
#elif defined(__gnu_linux__) || defined(__linux__)
#  define OS_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
#  define OS_MAC 1
#endif

#if defined(_MSC_VER)
#   define COMPILER_MSVC 1
#elif defined(__clang__)
#   define COMPILER_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
#   define COMPILER_GCC 1
#endif

#if defined(OS_UNIX)
#   include <unistd.h>
#endif

#if defined(COMPILER_MSVC)
#   include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#if COMPILER_MSVC
#   define cflat_trap() __debugbreak()
#elif COMPILER_CLANG || COMPILER_GCC
#   define cflat_trap() __builtin_trap()
#else
#   define cflat_trap() asm("int3")
#endif

#if !defined(cflat_assert)
#   include <assert.h>
#   define cflat_assert assert
#endif
#if C23_OR_GREATER
#   define cflat_typeof typeof
#   define cflat_alignof alignof
#else
#   define cflat_typeof __typeof__
#   define cflat_alignof(T) __alignof__(T)
#endif
#if COMPILER_MSVC
#   if defined(__SANITIZE_ADDRESS__)
#       define ASAN_ENABLED 1
#   endif
#elif COMPILER_CLANG
#   if defined(__has_feature)
#       if __has_feature(address_sanitizer)
#           define ASAN_ENABLED 1
#       endif
#   elif defined(__SANITIZE_ADDRESS__)
#       define ASAN_ENABLED 1
#   endif
#elif COMPILER_GCC
    #if defined(__SANITIZE_ADDRESS__)
#       define ASAN_ENABLED 1
#   endif
#else
#   error alignof not defined for this compiler.
#endif

#if !defined (C23_OR_GREATER)
#   define cflat_enum(ENUM_NAME, ENUM_TYPE) typedef ENUM_TYPE ENUM_NAME; enum
#else
#   define cflat_enum(ENUM_NAME, ENUM_TYPE) typedef ENUM_TYPE ENUM_NAME; enum
#endif

#define cflat_not_implemented cflat_assert(!"Not Implemented!")
#define cflat_align_pow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))
#define cflat_is_pow2(x) ((x)!=0 && ((x)&((x)-1))==0)

#define cflat_next_pow2(x) _Generic((x),    \
    i8: cflat_next_pow2_i8,                 \
    u8: cflat_next_pow2_u8,                 \
    u16: cflat_next_pow2_u16,               \
    u32: cflat_next_pow2_u32,               \
    u64: cflat_next_pow2_u64,               \
    i16: cflat_next_pow2_i16,               \
    i32: cflat_next_pow2_i32,               \
    i64: cflat_next_pow2_i64                \
)(x)

#define cflat_prev_pow2(x) _Generic((x),    \
    i8: cflat_prev_pow2_i8,                 \
    u8: cflat_prev_pow2_u8,                 \
    i16: cflat_prev_pow2_i16,               \
    u16: cflat_prev_pow2_u16,               \
    i32: cflat_prev_pow2_i32,               \
    u32: cflat_prev_pow2_u32,               \
    i64: cflat_prev_pow2_i64,               \
    u64: cflat_prev_pow2_u64                \
)(x)

#define cflat_sizeof_member(T, member) sizeof ((T*)0)->member
#define cflat_static_assert(e) (void)sizeof(struct {char _; static_assert (e, ""); })

#define CFLAT_ARRAY_SIZE(XS) (sizeof((XS))/sizeof((XS)[0]))
#define CFLAT_ROW_SIZE(XS) CFLAT_ARRAY_SIZE(XS)
#define CFLAT_COL_SIZE(XS) (sizeof((XS))/sizeof((XS)[0][0]) / CFLAT_ROW_SIZE(XS))

#define cflat_min(A,B) (((A)<(B))?(A):(B))
#define cflat_max(A,B) (((A)>(B))?(A):(B))
#define cflat_abs(A) (((A) < 0) ? (-(A)) : ((A)))

#define cflat_assert_type_member(T, member) (void)cflat_static_assert(cflat_sizeof_member(T,member), "")
#define cflat_assert_ptr_has_member(p, member) (void)cflat_static_assert(sizeof(p)->member, "")
#define cflat_assert_has_member(p, member) (void)cflat_static_assert(sizeof(p).member, "")

#define CONCAT_(a,b) a ## b
#define CONCAT(a,b) CONCAT_(a,b)

#define cflat_defer(begin, end) \
    for(int CONCAT(_i, __LINE__) = 0; CONCAT(_i, __LINE__) == 0; CONCAT(_i, __LINE__)++) \
    for(begin; CONCAT(_i, __LINE__) == 0; CONCAT(_i, __LINE__)++, end)

#define cflat_defer_exit continue

#define cflat_ll_push(top,node,link) ((node)->link=(top), (top)=(node))
#define cflat_ll_pop(top, link) ((top)=(top)->link)

#define cflat_swap(a, b) do { cflat_typeof((a)) t__ = a; a = b; b = t__;} while(0)

#define cflat_define_padded_struct(type, size) union {                                                                 \
    byte raw[sizeof(type) + (size)];                                                                                   \
    type structure;                                                                                                    \
}

#define cflat_padded_struct(type, size, ...) &(cflat_define_padded_struct(type, size)) {                               \
    .structure={ __VA_ARGS__ }                                                                                         \
}.structure

#include <string.h>
#include <stdlib.h>
#define cflat_mem_copy memcpy
#define cflat_mem_zero(mem, len) memset(mem, 0, len)

#define cflat_lit(LITERAL) (*((cflat_typeof(LITERAL)[1]) {LITERAL}))
#define cflat_plit(LITERAL) (((cflat_typeof(LITERAL)[1]) {LITERAL}))

#if !defined(container_of)
#   define container_of(ptr, type, member) ((type *) ((byte *)(ptr) - offsetof(type, member)))
#endif

#if defined(NO_BOUNDS_CHECK)
#   define cflat_bounds_check(index, len) ((void)(index), (void)(len))
#else
#   define cflat_bounds_check(index, len) cflat_assert((usize)(index) < (usize)(len) && "Index out of bounds")
#endif

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define OVERRIDE_INIT(CLITERAL, ...)                            \
__extension__  ({                                               \
    _Pragma("GCC diagnostic push")                              \
    _Pragma("GCC diagnostic ignored \"-Woverride-init\"")       \
    ((CLITERAL) {__VA_ARGS__});                                 \
    _Pragma("GCC diagnostic pop")                               \
})
#else
#   define OVERRIDE_INIT(CLITERAL, ...) ((CLITERAL) {__VA_ARGS__})
#endif

#if !defined(CFLAT_NO_TYPEDEF_STDINT)
// Unsigned
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
// Signed
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
// Floating point
#if __SIZEOF_FLOAT__ == 4
typedef float f32;
#endif
#if __SIZEOF_DOUBLE__ == 8
typedef double f64;
#endif
#if __SIZEOF_LONG_DOUBLE__ == 16
typedef long double f128;
#elif __SIZEOF_LONG_DOUBLE__ == 10
typedef long double f80;
#endif
// Size
typedef size_t usize;
typedef ssize_t isize;
// Pointer Arithmetic
typedef intptr_t iptr;
typedef uintptr_t uptr;
typedef ptrdiff_t ptrdiff;
// Memory
typedef u8 byte;
#endif // CFLAT_NO_TYPEDEF_STDINT

i8 (cflat_next_pow2_i8)(i8 x);
u8 (cflat_next_pow2_u8)(u8 x);
i16 (cflat_next_pow2_i16)(i16 x);
u16 (cflat_next_pow2_u16)(u16 x);
i32 (cflat_next_pow2_i32)(i32 x);
u32 (cflat_next_pow2_u32)(u32 x);
i64 (cflat_next_pow2_i64)(i64 x);
u16 (cflat_next_pow2_u16)(u16 x);

#ifndef KiB
#   define KiB(x) ((x) << 10)
#endif
#ifndef MiB
#   define MiB(x) ((x) << 20)
#endif
#ifndef GiB
#   define GiB(x) ((x) << 30)
#endif

#ifndef K
#   define K(x) ((x) * 1000)
#endif

#if COMPILER_MSVC
#   define cflat_thread_local __declspec(thread)
#else
#   define cflat_thread_local _Thread_local
#endif

#if defined(OS_WINDOWS)
#   define cflat_export __declspec(dllexport)
#elif defined(OS_UNIX)
#   define cflat_export __attribute__((visibility("default")))
#endif

#if defined(CFLAT_IMPLEMENTATION)

i8 (cflat_is_pow2_i8)(i8 x) {
    if (x <= 1) return 1;
    x--;
    x |= x >> 1;
    return ++x;
}

u8 (cflat_next_pow2_u8)(u8 x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    return ++x;
}

i16 (cflat_is_pow2_i16)(i16 x) {
    if (x <= 1) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    return ++x;
}

u16 (cflat_next_pow2_u16)(u16 x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    return ++x;
}

i32 (cflat_next_pow2_i32)(i32 x) {
    if (x <= 1) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return ++x;
}

u32 (cflat_next_pow2_u32)(u32 x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return ++x;
}

i64 (cflat_next_pow2_i64)(i64 x) {
    if (x <= 1) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return ++x;
}

u64 (cflat_next_pow2_u64)(u64 x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return ++x;
}

i8 (cflat_prev_pow2_i8)(i8 x)
{
    if (x <= 0) return 0;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    return x - (x >> 1);
}

u8 (cflat_prev_pow2_u8)(u8 x)
{
    if (x == 0) return 0;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    return x - (x >> 1);
}

i16 (cflat_prev_pow2_i16)(i16 x)
{
    if (x <= 0) return 0;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    return x - (x >> 1);
}

u16 (cflat_prev_pow2_u16)(u16 x)
{
    if (x == 0) return 0;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    return x - (x >> 1);
}

i32 (cflat_prev_pow2_i32)(i32 x)
{
    if (x <= 0) return 0;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x - (x >> 1);
}

u32 (cflat_prev_pow2_u32)(u32 x)
{
    if (x == 0) return 0;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x - (x >> 1);
}

i64 (cflat_prev_pow2_i64)(i64 x)
{
    if (x <= 0) return 0;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x - (x >> 1);
}

u64 (cflat_prev_pow2_u64)(u64 x)
{
    if (x == 0) return 0;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x - (x >> 1);
}

#endif // CFLAT_IMPLEMENTATION

#if !defined(CFLAT_NO_ALIAS)
#   if !defined(assert)
#       define assert cflat_assert
#   endif
#   if !defined (C23_OR_GREATER)
#       define alignof cflat_alignof
#   endif
#   define not_implemented cflat_not_implemented
#   define align_pow2 cflat_align_pow2
#   define is_pow2 cflat_is_pow2
#   define next_pow2 cflat_next_pow2
#   define prev_pow2 cflat_prev_pow2
#   define log_expression cflat_log_expression
#   ifndef min
#       define min cflat_min
#   endif
#   ifndef max
#       define max cflat_max
#   endif
#   define trap cflat_trap
#   define defer cflat_defer
#   define defer_exit cflat_defer_exit
#   define exit_defer cflat_defer_exit
#   define ll_push cflat_ll_push
#   define ll_pop cflat_ll_pop
#   define swap cflat_swap
#   ifndef ARRAY_SIZE
#       define ARRAY_SIZE CFLAT_ARRAY_SIZE
#   endif
#   define mem_copy cflat_mem_copy
#   define mem_zero cflat_mem_zero
#endif // CFLAT_NO_ALIAS

#endif //CFLAT_DEF_H
