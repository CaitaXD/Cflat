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
#   define cflat_assert(x) assert(x)
#endif
#if __STDC_VERSION__ >= 202311L
#   define cflat_typeof typeof
#else
#   define cflat_typeof __typeof__
#endif
#if COMPILER_MSVC
#   define cflat_alignof(T) __alignof(T)
#elif COMPILER_CLANG
#   define cflat_alignof(T) __alignof(T)
#elif COMPILER_GCC
#   define cflat_alignof(T) __alignof__(T)
#else
#   error alignof not defined for this compiler.
#endif

#define enum(ENUM_NAME, ENUM_TYPE) typedef ENUM_TYPE ENUM_NAME; enum
#define cflat_not_implemented cflat_assert(!"Not Implemented!")
#define cflat_align_pow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))
#define cflat_sizeof_member(T, member) sizeof ((T)0)->member
#define cflat_static_assert(e,msg) (void)sizeof(struct {char _; static_assert (e,msg); })
#define cflat_min(A,B) (((A)<(B))?(A):(B))
#define cflat_max(A,B) (((A)>(B))?(A):(B))
#define cflat_abs(A) (((A) < 0) ? (-(A)) : ((A)))

#define cflat_assert_type_member(T, member) (void)cflat_static_assert(cflat_sizeof_member(T,member), "")
#define cflat_assert_ptr_has_member(p, member) (void)cflat_static_assert(sizeof(p)->member, "")
#define cflat_assert_has_member(p, member) (void)cflat_static_assert(sizeof(p).member, "")

#define cflat_defer(begin, end) \
    for(int _defer_latch = 0; _defer_latch == 0; _defer_latch++, end) \
    for(begin; _defer_latch == 0; _defer_latch++)

#define cflat_ll_push(top,node,link) ((node)->link=(top), (top)=(node))
#define cflat_ll_pop(top, link) ((top)=(top)->link)

#define cflat_swap(a, b) do { cflat_typeof((a)) t__ = a; a = b; b = t__;} while(0)

#if !defined(CFLAT_NO_CRT)
#   include <string.h>
#   include <stdlib.h>
#   define cflat_mem_copy memcpy
#   define cflat_mem_zero(mem, len) memset(mem, 0, len)
#endif
// TODO: No Crt Fallbacks

#if !defined(CFLAT_NO_ALIAS)
#if !defined(assert)
#   define assert cflat_assert
#endif
#if COMPILER_MSVC
#   undef min
#   undef max
#endif
#   define notImplemented cflat_not_implemented
#   define align_pow2 cflat_align_pow2
#   define alignof cflat_alignof
#   define min cflat_min
#   define max cflat_max
#   define trap cflat_trap
#   define defer cflat_defer
#   define ll_push cflat_ll_push
#   define ll_pop cflat_ll_pop
#   define swap cflat_swap
#endif // CFLAT_NO_ALIAS

#if !defined(container_of)
#   define container_of(ptr, type, member) ((type *) ((byte *)(ptr) - offsetof(type, member)))
#endif

#if !defined(CFLAT_NO_TYPEDEF)
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
typedef float f32;
typedef double f64;
// Size
typedef size_t usize;
typedef ssize_t isize;
// Pointer Arithmetic
typedef intptr_t iptr;
typedef uintptr_t uptr;
typedef ptrdiff_t ptrdiff;
// Memory
typedef u8 byte;
#endif // CFLAT_NO_TYPEDEF

#ifndef KiB
#   define KiB(x) (x << 10)
#endif
#ifndef MiB
#   define MiB(x) (x << 20)
#endif
#ifndef GiB
#   define GiB(x) (x << 30)
#endif

#if COMPILER_MSVC
#   define cflat_thread_local __declspec(thread)
#else
#   define cflat_thread_local _Thread_local
#endif

#endif //CFLAT_DEF_H