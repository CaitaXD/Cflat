#ifndef CFLAT_AVX_H
#define CFLAT_AVX_H

#include "CflatCore.h"
#include "CflatMath.h"

#if defined(__AVX__)
#   include <immintrin.h>
#endif

#define cflat_vec_length(TVec)  _Generic(*((TVec*)0)        \
    , CflatVec256f:  8                                      \
    , CflatVec256cf: 4                                      \
    , CflatVec256i:  8                                      \
    , CflatVec256d:  4                                      \
)

#define cflat_for_vec(TVec, OFFSET, LEN) for (; (OFFSET) + cflat_vec_length(TVec) <= (LEN); (OFFSET) += cflat_vec_length(TVec))

typedef struct cflat_vec256f {
    #if defined (__AVX__)
    cflat_alignas(32) __m256 v;
    #else
    cflat_alignas(32) f32 v[8];
    #endif
} CflatVec256f;

typedef struct cflat_vec256cf {
    #if defined (__AVX__)
    cflat_alignas(32) __m256 v;
    #else
    cflat_alignas(32) c32 v[4];
    #endif
} CflatVec256cf;

typedef struct cflat_vec256d {
    #if defined (__AVX__)
    cflat_alignas(32) __m256d v;
    #else
    cflat_alignas(32) f64 v[4];
    #endif
} CflatVec256d;

typedef struct cflat_vec256i {
    #if defined (__AVX__)
    cflat_alignas(32) __m256i v;
    #else
    cflat_alignas(32) i32 v[8];
    #endif
} CflatVec256i;

CFLAT_DEF CflatVec256f  cflat_v256f(f32 x, f32 y, f32 z, f32 w, f32 v, f32 u, f32 t, f32 s);
CFLAT_DEF CflatVec256cf cflat_v256cf(c32 x, c32 y, c32 z, c32 w);
CFLAT_DEF CflatVec256cf cflat_v256cff(f32 xr, f32 xi, f32 yr, f32 yi, f32 zr, f32 zi, f32 wr, f32 wi);

CFLAT_DEF CflatVec256cf cflat_load_v256cf(const c32 *src);
CFLAT_DEF CflatVec256cf cflat_load_unaligned_v256cf(const c32 *src);
CFLAT_DEF c32*          cflat_store_v256cf(c32 *dst, CflatVec256cf src);
CFLAT_DEF c32*          cflat_store_unaligned_v256cf(c32 *dst, CflatVec256cf src);

CFLAT_DEF CflatVec256f  cflat_load_v256f(const f32 *src);
CFLAT_DEF CflatVec256f  cflat_load_unaligned_v256f(const f32 *src);
CFLAT_DEF f32*          cflat_store_v256f(f32 *dst, CflatVec256f src);
CFLAT_DEF f32*          cflat_store_unaligned_v256f(f32 *dst, CflatVec256f src);

CFLAT_DEF CflatVec256cf cflat_broadcast_v256cf(c32 x);
CFLAT_DEF CflatVec256cf cflat_broadcast_v256cff(f32 real, f32 imag);
CFLAT_DEF CflatVec256cf cflat_conj_v256cf(CflatVec256cf x);
CFLAT_DEF CflatVec256cf cflat_mul_v256cf(CflatVec256cf cx, CflatVec256cf cy);
CFLAT_DEF CflatVec256cf cflat_add_v256cf(CflatVec256cf x, CflatVec256cf y);
CFLAT_DEF CflatVec256cf cflat_sub_v256cf(CflatVec256cf x, CflatVec256cf y);
CFLAT_DEF CflatVec256cf cflat_scale_v256cff(CflatVec256cf x, f32 s);
CFLAT_DEF CflatVec256cf cflat_mul_v256cff(CflatVec256cf x, CflatVec256f s);

CFLAT_DEF CflatVec256f cflat_broadcast_v256f(f32 x);
CFLAT_DEF CflatVec256f cflat_mul_v256f(CflatVec256f x, CflatVec256f y);
CFLAT_DEF CflatVec256f cflat_add_v256f(CflatVec256f x, CflatVec256f y);
CFLAT_DEF CflatVec256f cflat_sub_v256f(CflatVec256f x, CflatVec256f y);
CFLAT_DEF CflatVec256f cflat_scale_v256f(CflatVec256f x, f32 s);

#if defined(CFLAT_IMPLEMENTATION)

/* ============================================================================================== */
/*                                 CflatVec256cf                                                  */
/* ============================================================================================== */

CflatVec256cf cflat_v256cf(c32 x, c32 y, c32 z, c32 w) {
    #if defined(__AVX__)
    return (CflatVec256cf){ .v = _mm256_set_ps(crealf(x), cimagf(x), crealf(y), cimagf(y), crealf(z), cimagf(z), crealf(w), cimagf(w)) };
    #else
    CflatVec256cf res;
    res.v[0] = x;
    res.v[1] = y;
    res.v[2] = z;
    res.v[3] = w;
    return res;
    #endif
}

CflatVec256cf cflat_v256cff(f32 xr, f32 xi, f32 yr, f32 yi, f32 zr, f32 zi, f32 wr, f32 wi) {
    #if defined(__AVX__)
    return (CflatVec256cf){ .v = _mm256_set_ps(xr, xi, yr, yi, zr, zi, wr, wi) };
    #else
    CflatVec256cf res;
    res.v[0] = xr + I*xi;
    res.v[1] = yr + I*yi;
    res.v[2] = zr + I*zi;
    res.v[3] = wr + I*wi;
    return res;
    #endif
}

CflatVec256cf cflat_load_v256cf(const c32 *src) {
    #if defined(__AVX__)
    return (CflatVec256cf){ .v = _mm256_load_ps((const f32*)src) };
    #else
    CflatVec256cf res;
    cflat_mem_copy(res.v, src, sizeof(res.v));
    return res;
    #endif
}

CflatVec256cf cflat_load_unaligned_v256cf(const c32 *src) {
    #if defined(__AVX__)
    return (CflatVec256cf){ .v = _mm256_loadu_ps((const f32*)src) };
    #else
    CflatVec256cf res;
    cflat_mem_copy(res.v, src, sizeof(res.v));
    return res;
    #endif
}

c32* cflat_store_v256cf(c32 *dst, CflatVec256cf src) {
    #if defined(__AVX__)
    _mm256_store_ps((f32*)dst, src.v);
    return dst;
    #else
    return cflat_mem_copy(dst, src.v, sizeof(src.v));
    #endif
}

CFLAT_DEF CflatVec256cf cflat_broadcast_v256cf(c32 x) {
    #if defined(__AVX__)
    f64 f = cflat_bit_cast(f64, c32, x);
    return (CflatVec256cf){ _mm256_castpd_ps(_mm256_set1_pd(f)) };
    #else
    return (CflatVec256cf){ .v = {x, x, x, x} };
    #endif
}

CFLAT_DEF CflatVec256cf cflat_broadcast_v256cff(f32 real, f32 imag) {
    #if defined(__AVX__)
    f64 f = cflat_bit_cast(f64, c32, (c32){ real + imag*I });
    return (CflatVec256cf){ _mm256_castpd_ps(_mm256_set1_pd(f)) }; 
    #else
    c32 x = real + imag*I;
    return (CflatVec256cf){ .v = {x, x, x, x} };
    #endif
}

CflatVec256cf cflat_conj_v256cf(CflatVec256cf x) {
    #if defined(__AVX__)
    __m256 mask = _mm256_set_ps(-0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f);
    return (CflatVec256cf) { .v = _mm256_xor_ps(x.v, mask) };
    #else
    CflatVec256cf res;
    for (usize i = 0; i < cflat_vec_length(CflatVec256cf); ++i) res.v[i] = conjf(x.v[i]);
    return res;
    #endif
}

CflatVec256cf cflat_mul_v256cf(CflatVec256cf cx, CflatVec256cf cy) {
    #if defined(__AVX__)
    __m256 real   = _mm256_moveldup_ps(cy.v);          
    __m256 imag   = _mm256_movehdup_ps(cy.v);          
    __m256 ap     = _mm256_permute_ps(cx.v, 0xB1);
    __m256 res    = _mm256_mul_ps(cx.v, real);
    return (CflatVec256cf) { .v = _mm256_addsub_ps(res, _mm256_mul_ps(ap, imag)) };
    #else
    CflatVec256cf res;
    for (usize i = 0; i < cflat_vec_length(CflatVec256cf); ++i) res.v[i] = cx.v[i] * cy.v[i];
    return res;
    #endif
}

CflatVec256cf cflat_add_v256cf(CflatVec256cf x, CflatVec256cf y) {
    #if defined(__AVX__)
    return (CflatVec256cf){ .v = _mm256_add_ps(x.v, y.v) };
    #else
    CflatVec256cf res;
    for(usize i = 0; i < cflat_vec_length(CflatVec256cf); ++i) res.v[i] = x.v[i] + y.v[i];
    return res;
    #endif
}

CflatVec256cf cflat_sub_v256cf(CflatVec256cf x, CflatVec256cf y) {
    #if defined(__AVX__)
    return (CflatVec256cf){ .v = _mm256_sub_ps(x.v, y.v) };
    #else
    CflatVec256cf res;
    for(usize i = 0; i < cflat_vec_length(CflatVec256cf); ++i) res.v[i] = x.v[i] - y.v[i];
    return res;
    #endif
}

CflatVec256cf cflat_scale_v256cff(CflatVec256cf x, f32 s) {
    #if defined(__AVX__)
    return (CflatVec256cf){ .v = _mm256_mul_ps(x.v, _mm256_set1_ps(s)) };
    #else
    CflatVec256cf res;
    for(usize i = 0; i < cflat_vec_length(CflatVec256cf); ++i) res.v[i] = x.v[i] * s;
    return res;
    #endif
}

CflatVec256cf cflat_mul_v256cff(CflatVec256cf x, CflatVec256f s) {
    #if defined(__AVX__)
    return (CflatVec256cf){ .v = _mm256_mul_ps(x.v, s.v) };
    #else
    CflatVec256cf res;
    for(usize i = 0; i < cflat_vec_length(CflatVec256f); ++i) ((f32*)res.v)[i] = ((f32*)x.v)[i] * ((f32*)s.v)[i];
    return res;
    #endif
}

/* ============================================================================================== */
/*                                 CflatVec256f                                                   */
/* ============================================================================================== */

CflatVec256f cflat_load_v256f(const f32 *src) {
    #if defined(__AVX__)
    return (CflatVec256f){ .v = _mm256_load_ps(src) };
    #else
    CflatVec256f res;
    cflat_mem_copy(res.v, src, sizeof(res.v));
    return res;
    #endif
}

CflatVec256f cflat_load_unaligned_v256f(const f32 *src) {
    #if defined(__AVX__)
    return (CflatVec256f){ .v = _mm256_loadu_ps(src) };
    #else
    CflatVec256f res;
    cflat_mem_copy(res.v, src, sizeof(res.v));
    return res;
    #endif
}

f32* cflat_store_v256f(f32 *dst, CflatVec256f src) {
    #if defined(__AVX__)
    _mm256_store_ps(dst, src.v);
    return dst;
    #else
    return cflat_mem_copy(dst, src.v, sizeof(src.v));
    #endif
}

f32* cflat_store_unaligned_v256f(f32 *dst, CflatVec256f src) {
    #if defined(__AVX__)
    _mm256_storeu_ps(dst, src.v);
    return dst;
    #else
    return cflat_mem_copy(dst, src.v, sizeof(src.v));
    #endif
}

CflatVec256f cflat_v256f(f32 x, f32 y, f32 z, f32 w, f32 v, f32 u, f32 t, f32 s) {
    #if defined(__AVX__)
    return (CflatVec256f){ .v = _mm256_set_ps(x, y, z, w, v, u, t, s) };
    #else
    CflatVec256f res;
    res.v[0] = x;
    res.v[1] = y;
    res.v[2] = z;
    res.v[3] = w;
    res.v[4] = v;
    res.v[5] = u;
    res.v[6] = t;
    res.v[7] = s;
    return res;
    #endif
}

CflatVec256f cflat_broadcast_v256f(f32 x) {
    #if defined(__AVX__)
    return (CflatVec256f){ .v = _mm256_set1_ps(x) };
    #else
    CflatVec256f res;
    for (usize i = 0; i < 8; ++i) res.v[i] = x;
    return res;
    #endif
}

CflatVec256f cflat_mul_v256f(CflatVec256f x, CflatVec256f y) {
    #if defined(__AVX__)
    return (CflatVec256f){ .v = _mm256_mul_ps(x.v, y.v) };
    #else
    CflatVec256f res;
    for(usize i = 0; i < cflat_vec_length(CflatVec256cf); ++i) res.v[i] = x.v[i] * y.v[i];
    return res;
    #endif
}

CflatVec256f cflat_add_v256f(CflatVec256f x, CflatVec256f y) {
    #if defined(__AVX__)
    return (CflatVec256f){ .v = _mm256_add_ps(x.v, y.v) };
    #else
    CflatVec256f res;
    for(usize i = 0; i < cflat_vec_length(CflatVec256f); ++i) res.v[i] = x.v[i] + y.v[i];
    return res;
    #endif
}

CflatVec256f cflat_sub_v256f(CflatVec256f x, CflatVec256f y) {
    #if defined(__AVX__)
    return (CflatVec256f){ .v = _mm256_sub_ps(x.v, y.v) };
    #else
    CflatVec256f res;
    for(usize i = 0; i < cflat_vec_length(CflatVec256f); ++i) res.v[i] = x.v[i] - y.v[i];
    return res;
    #endif
}

CflatVec256f cflat_scale_v256f(CflatVec256f x, f32 s) {
    #if defined(__AVX__)
    return (CflatVec256f){ .v = _mm256_mul_ps(x.v, _mm256_set1_ps(s)) };
    #else
    CflatVec256f res;
    for(usize i = 0; i < cflat_vec_length(CflatVec256f); ++i) res.v[i] = x.v[i] * s;
    return res;
    #endif
}

#endif // CFLAT_IMPLEMENTATION

#if !defined(CFLAT_AVX_NO_ALIAS)

#define Vec256f CflatVec256f
#define Vec256cf CflatVec256cf
#define Vec256d CflatVec256d
#define Vec256i CflatVec256i

#define v256f cflat_v256f
#define v256cf cflat_v256cf
#define v256cff cflat_v256cff

#define load_v256cf cflat_load_v256cf
#define store_v256cf cflat_store_v256cf

#define load_v256f cflat_load_v256f
#define store_v256f cflat_store_v256f

#define conj_v256cf cflat_conj_v256cf
#define mul_v256cf cflat_mul_v256cf
#define add_v256cf cflat_add_v256cf
#define sub_v256cf cflat_sub_v256cf
#define scale_v256cff cflat_scale_v256cff
#define mul_v256cff cflat_mul_v256cff

#define broadcast_v256f cflat_broadcast_v256f
#define mul_v256f cflat_mul_v256f
#define add_v256f cflat_add_v256f
#define sub_v256f cflat_sub_v256f

#endif //CFLAT_AVX_NO_ALIAS

#endif //CFLAT_AVX_H