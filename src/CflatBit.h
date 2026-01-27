#ifndef CFLAT_BIT_H
#define CFLAT_BIT_H

#include "CflatCore.h"

CFLAT_DEF i8 (cflat_next_pow2_i8) (i8  x);
CFLAT_DEF u8 (cflat_next_pow2_u8) (u8  x);
CFLAT_DEF i16(cflat_next_pow2_i16)(i16 x);
CFLAT_DEF u16(cflat_next_pow2_u16)(u16 x);
CFLAT_DEF i32(cflat_next_pow2_i32)(i32 x);
CFLAT_DEF u32(cflat_next_pow2_u32)(u32 x);
CFLAT_DEF i64(cflat_next_pow2_i64)(i64 x);
CFLAT_DEF u64(cflat_next_pow2_u64)(u64 x);

CFLAT_DEF i8 (cflat_prev_pow2_i8) (i8  x);
CFLAT_DEF u8 (cflat_prev_pow2_u8) (u8  x);
CFLAT_DEF i16(cflat_prev_pow2_i16)(i16 x);
CFLAT_DEF u16(cflat_prev_pow2_u16)(u16 x);
CFLAT_DEF i32(cflat_prev_pow2_i32)(i32 x);
CFLAT_DEF u32(cflat_prev_pow2_u32)(u32 x);
CFLAT_DEF i64(cflat_prev_pow2_i64)(i64 x);
CFLAT_DEF u64(cflat_prev_pow2_u64)(u64 x);

CFLAT_DEF u32(cflat_log2_u32)(u32 x);
CFLAT_DEF u64(cflat_log2_u64)(u64 x);

#define cflat_align_pow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))
#define cflat_is_pow2(x) ((x)!=0 && ((x)&((x)-1))==0)

#define cflat_next_pow2(x) _Generic((x),\
    i8: cflat_next_pow2_i8,            \
    u8: cflat_next_pow2_u8,            \
    u16: cflat_next_pow2_u16,          \
    u32: cflat_next_pow2_u32,          \
    u64: cflat_next_pow2_u64,          \
    i16: cflat_next_pow2_i16,          \
    i32: cflat_next_pow2_i32,          \
    i64: cflat_next_pow2_i64           \
)(x)

#define cflat_prev_pow2(x) _Generic((x),\
    i8: cflat_prev_pow2_i8,            \
    u8: cflat_prev_pow2_u8,            \
    i16: cflat_prev_pow2_i16,          \
    u16: cflat_prev_pow2_u16,          \
    i32: cflat_prev_pow2_i32,          \
    u32: cflat_prev_pow2_u32,          \
    i64: cflat_prev_pow2_i64,          \
    u64: cflat_prev_pow2_u64           \
)(x)

#if defined(CFLAT_IMPLEMENTATION)

i8 (cflat_next_pow2_i8)(i8 x) {
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

i16 (cflat_next_pow2_i16)(i16 x) {
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

cflat_alignas(64) const i8 cflat__log2_lut_64[64] = {
    63,0,58,1,59,47,53,2,
    60,39,48,27,54,33,42,3,
    61,51,37,40,49,18,28,20,
    55,30,34,11,43,14,22,4,
    62,57,46,52,38,26,32,41,
    50,36,17,19,29,10,13,21,
    56,45,25,31,35,16,9,12,
    44,24,15,8,23,7,6,5
};

cflat_alignas(64) const i8 cflat__log2_lut_32[32] = {
    0 ,9,1,10,13,21,2,29,
    11,14,16,18,22,25,3,30,
    8 ,12,20,28,15,17,24,7,
    19,27,23,6,26,5,4,31
};


u64 cflat_log2_u64(u64 x) {
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return cflat__log2_lut_64[((uint64_t)((x - (x >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}

u32 cflat_log2_u32(u32 x) {
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return cflat__log2_lut_32[(u32)(x*0x07C4ACDD) >> 27];
}

#endif // CFLAT_IMPLEMENTATION

#if !defined(CFLAT_NO_ALIAS)
#   define align_pow2 cflat_align_pow2
#   define is_pow2 cflat_is_pow2
#   define next_pow2 cflat_next_pow2
#   define prev_pow2 cflat_prev_pow2
#endif // CFLAT_NO_ALIAS

#endif //CFLAT_BIT_H