#ifndef CFLAT_DEF_H
#define CFLAT_DEF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifndef CFLAT_DEF
#   define CFLAT_DEF
#endif

/* 

+============================================================================+
|                              C Standard                                    |
+============================================================================+

*/

#if __STDC_VERSION__ >= 202311L
#   define C23_OR_GREATER 1
#endif
#if __STDC_VERSION__ >= 201112L
#   define C11_OR_GREATER 1
#endif
#if __STDC_VERSION__ >= 199901L
#   define C99_OR_GREATER 1
#endif

/*

+============================================================================+
|                               Operating Systems                            |
+============================================================================+

*/

#if defined(__unix__)
#   define OS_UNIX 1
#   include <unistd.h>
#endif

#if defined(_WIN32)
#   define OS_WINDOWS 1
#elif defined(__gnu_linux__) || defined(__linux__)
#   define OS_LINUX 1
#elif defined(__APPLE__) && defined(__MACH__)
#   warning "Good luck lmao!"
#   define OS_MAC 1
#endif

/*

+============================================================================+
|                               Compilers                                    |
+============================================================================+

*/

#if defined(_MSC_VER)
#   define COMPILER_MSVC 1
#   include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#elif defined(__clang__)
#   define COMPILER_CLANG 1
#elif defined(__GNUC__) || defined(__GNUG__)
#   define COMPILER_GCC 1
#endif

/*

+============================================================================+
|                               Debug Trap                                   |
+============================================================================+

*/

#if COMPILER_MSVC
#   define cflat_trap() __debugbreak()
#elif defined(COMPILER_CLANG) || defined(COMPILER_GCC)
#   define cflat_trap() __builtin_trap()
#else
#   define cflat_trap() asm("int3")
#endif

/*

+============================================================================+
|                               Assert                                       |
+============================================================================+

*/

#define cflat_static_assert(e) (void)sizeof(struct {char _; static_assert (e, ""); })

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)

#   define cflat_assert(e) __extension__  ({                                            \
    if (!(e)) {                                                                         \
        fprintf(stderr, "Assertion Failed (%s:%d) %s ", __FILE__, __LINE__, __func__);   \
        puts(#e);                                                                       \
        cflat_trap();                                                                   \
    }                                                                                   \
})

# else
#   include <assert.h>
#   define cflat_assert assert
# endif

#define cflat_not_implemented cflat_assert(!"Not Implemented!")
#define cflat_unreachable cflat_assert(!"Unreachable!")

/* 

+============================================================================+
|                               Address Sanitizer                            |
+============================================================================+

*/

#if defined(COMPILER_MSVC)
#   if defined(__SANITIZE_ADDRESS__)
#       define ASAN_ENABLED 1
#   endif
#elif defined(COMPILER_CLANG)
#   if defined(__has_feature)
#       if __has_feature(address_sanitizer)
#           define ASAN_ENABLED 1
#       endif
#   elif defined(__SANITIZE_ADDRESS__)
#       define ASAN_ENABLED 1
#   endif
#elif defined(COMPILER_GCC)
    #if defined(__SANITIZE_ADDRESS__) && !defined(OS_WINDOWS) // Mingw doesn't seem to support ASAN
#       define ASAN_ENABLED 1
#   endif
#endif

/* 

+============================================================================+
|                               C Array Size                                 |
+============================================================================+

*/

#define CFLAT_ARRAY_SIZE(XS) (sizeof((XS))/sizeof((XS)[0]))
#define CFLAT_ROW_SIZE(XS) CFLAT_ARRAY_SIZE(XS)
#define CFLAT_COL_SIZE(XS) (sizeof((XS))/sizeof((XS)[0][0]) / CFLAT_ROW_SIZE(XS))

/*

+============================================================================+
|                                Macros                                      |
+============================================================================+

*/

#if defined(C23_OR_GREATER)
#   define cflat_typeof typeof
#   define cflat_alignof alignof
#   define cflat_alignas alignas
#else
#   define cflat_typeof __typeof__
#   define cflat_alignas _Alignas
#   define cflat_alignof __alignof__
#endif

#if defined(C23_OR_GREATER)
#   define cflat_enum(ENUM_NAME, ENUM_TYPE) typedef ENUM_TYPE ENUM_NAME; enum ENUM_NAME : ENUM_TYPE 
#else
#   define cflat_enum(ENUM_NAME, ENUM_TYPE) typedef ENUM_TYPE ENUM_NAME; enum
#endif

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#   define cflat_force_inline inline __attribute__((always_inline))
#elif defined(COMPILER_MSVC)
#   define cflat_force_inline inline __forceinline
#else
#   define cflat_force_inline inline
#endif

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#   define cflat_unlikely(x) __builtin_expect(!!(x), 0)
#   define cflat_likely(x)   __builtin_expect(!!(x), 1)
#else
#   define cflat_unlikely(x) (x)
#   define cflat_likely(x)   (x)
#endif

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#   define cflat_pause __builtin_ia32_pause
#elif defined(__x86_64__) || defined(__i386__)
#   include <immintrin.h>
#   define cflat_pause _mm_pause
#else
#   define cflat_pause
#endif

#define cflat_alignofexp(EXP) cflat_alignof(cflat_typeof(EXP))
#define cflat_sizeof_member(T, member)  sizeof       ( ( (T*) 0)->member             )
#define cflat_typeof_member(T, member)  cflat_typeof ( ( (T*) 0)->member             )
#define cflat_alignof_member(T, member) cflat_alignof(cflat_typeof_member(T, member) )

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
#define cflat_ll_pop(top,link)       ((top)=(top)->link)

#define cflat_swap(T, a, b) do { T t__ = a; a = b; b = t__;} while(0)

#define cflat_define_padded_struct(type, size) union {                                                                 \
    byte raw[sizeof(type) + (size)];                                                                                   \
    type structure;                                                                                                    \
}

#define cflat_padded_struct(type, size, ...) (cflat_define_padded_struct(type, size)) {                                \
    .structure={ __VA_ARGS__ }                                                                                         \
}.structure

#include <string.h>
#include <stdlib.h>
#define cflat_lvalue(TYPE, LITERAL) (*(TYPE[1]) {LITERAL})
#define cflat_mem_copy memcpy
#define cflat_mem_move memmove
#define cflat_mem_zero(MEM, LEN) memset((MEM), 0, (LEN))
#define cflat_bit_cast(TFrom, TTo, VAL) *(TTo*) cflat_mem_copy( &cflat_lvalue(TTo, 0), &cflat_lvalue(TFrom, (VAL)), sizeof(TTo))

#define cflat_lvalue_cast(TFROM, TTO) *(TTO *) (TFROM[1])


#if !defined(container_of)
#   define container_of(ptr, type, member) ((type *) ((byte *)(ptr) - offsetof(type, member)))
#endif

#if defined(NO_BOUNDS_CHECK)
#   define cflat_bounds_check(index, len) ((void)(len), (index))
#else
#   define cflat_bounds_check(index, len) (cflat_assert((usize)(index) < (usize)(len) && "Index out of bounds"), (index))
#endif

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define CFLAT_OPT(CLITERAL, ...)                                \
__extension__  ({                                               \
    _Pragma("GCC diagnostic push")                              \
    _Pragma("GCC diagnostic ignored \"-Woverride-init\"")       \
    ((CLITERAL) {__VA_ARGS__});                                 \
    _Pragma("GCC diagnostic pop")                               \
})
#else
#   define CFLAT_OPT(CLITERAL, ...) ((CLITERAL) {__VA_ARGS__})
#endif

#ifndef KiB
#   define KiB(x) ((x) << 10)
#endif
#ifndef MiB
#   define MiB(x) ((x) << 20)
#endif
#ifndef GiB
#   define GiB(x) ((x) << 30)
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

/*

+============================================================================+
|                               Typedefs                                     |
+============================================================================+

*/

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

#if defined(CFLAT_IMPLEMENTATION)

#endif // CFLAT_IMPLEMENTATION

#if !defined(CFLAT_NO_ALIAS)
#   if !defined(assert)
#       define assert cflat_assert
#   endif
#   if !defined (C23_OR_GREATER)
#       define alignof cflat_alignof
#   endif
#   define not_implemented cflat_not_implemented
#   if !defined (unreachable)
#       define unreachable cflat_unreachable
#   endif
#   if !defined (min)
#       define min cflat_min
#   endif
#   if !defined (max)
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
#   define ROW_SIZE CFLAT_ROW_SIZE
#   define COL_SIZE CFLAT_COL_SIZE
#   define mem_copy cflat_mem_copy
#   define mem_zero cflat_mem_zero
#   define lvalue cflat_lvalue
#   define lvalue_cast cflat_lvalue_cast
#endif // CFLAT_NO_ALIAS

#endif //CFLAT_DEF_H
