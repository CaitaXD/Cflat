#ifndef CFLAT_FFT_H
#define CFLAT_FFT_H

#include "CflatArena.h"
#include "CflatBit.h"
#include "CflatCore.h"
#include "CflatMath.h"
#include "CflatAVX.h"
#include "CflatLinear.h"
#include <complex.h>
#include <stdio.h>


void cflat_fft_c32(const usize fft_size, c32 *restrict out, const c32 *restrict in);
void cflat_ifft_c32(const usize fft_size, c32 *restrict out, const c32 *restrict in);

#if defined(CFLAT_IMPLEMENTATION)

/* ============================================================================================== */
/*                                 Twiddle Lookup Table                                           */
/* ============================================================================================== */

#define CFLAT_TWIDDLE_CACHE_LOG2 (16)
#define CFLAT_TWIDDLE_CACHE_SIZE (1 << CFLAT_TWIDDLE_CACHE_LOG2)

_Alignas(64) static cflat_thread_local c32 TWIDDLE_LUT[CFLAT_TWIDDLE_CACHE_SIZE];

cflat_force_inline u64 cflat_twidle_index(u64 k, u64 len) {
    const usize L = CFLAT_TWIDDLE_CACHE_SIZE;
    const usize n = CFLAT_TWIDDLE_CACHE_LOG2 - cflat_log2_u64(len);
    const usize S_n = L - (L >> n);
    return k + S_n;
}

c32 cflat_twidle_lookup_c32(u64 k, u64 len) {
    return TWIDDLE_LUT[cflat_twidle_index(k, len)];
}

CflatVec256cf cflat_twiddle_lookup_v256cf(u64 k, u64 len) {
    return cflat_load_v256cf(TWIDDLE_LUT + cflat_twidle_index(k, len));
}

void cflat_twidle_precompute(void) {
    static bool done = false;
    if (cflat_likely(done)) return;
    for (usize len = 1; len <= CFLAT_TWIDDLE_CACHE_SIZE; len <<= 1) 
    for (u64 k = 0; k < len/2; k += 1) {
        TWIDDLE_LUT[cflat_twidle_index(k, len)] = cexpf(I*-2.0*cflat_pi*(f64)k/(f64)len);
    }
    done = true;
}

/* ============================================================================================== */
/*                                 FFT Butterfly                                                  */
/* ============================================================================================== */

static void cflat__radix2merge_v256cf(usize k, usize fft_size, c32 *restrict out) {
    const usize half = fft_size/2;
    const usize e = k;           
    const usize o = k + half;
    CflatVec256cf ev = cflat_load_v256cf(out + e);                                                              // c32 ev = out[e];
    CflatVec256cf od = cflat_mul_v256cf(cflat_load_v256cf(out + o), cflat_twiddle_lookup_v256cf(k, fft_size));  // c32 od = out[o] * twiddle;
    cflat_store_v256cf(out + e, cflat_add_v256cf(ev, od));                                                      // out[e] = ev + od;
    cflat_store_v256cf(out + o, cflat_sub_v256cf(ev, od));                                                      // out[o] = ev - od;
}

static void clfat__radix1butterfly(const usize stride, c32 *restrict out, const c32 *restrict in) {
    (void)stride;
    out[0] = in[0];
}

static void cflat__radix2butterfly(const usize stride, c32 *restrict out, const c32 *restrict in) {
    c32 a = in[0*stride];
    c32 b = in[1*stride];
    out[0] = a + b;
    out[1] = a - b;
}

static void cflat__radix4butterfly(const usize stride, c32 *restrict out, const c32 *restrict in) {
    c32 x0 = in[0*stride];
    c32 x1 = in[1*stride];
    c32 x2 = in[2*stride];
    c32 x3 = in[3*stride];
    c32 t0 = x0 + x2;
    c32 t1 = x0 - x2;
    c32 t2 = x1 + x3;
    c32 t3 = x1 - x3;
    c32 t3_rot = t3 * -I;
    out[0] = t0 + t2;
    out[1] = t1 + t3_rot;
    out[2] = t0 - t2;
    out[3] = t1 - t3_rot;
}

#define INVSQRT2 (0.707106781186547524400844362104849039284835937688474036588339868)

static void  cflat__radix8butterfly(const usize stride, c32 *restrict out, const c32 *restrict in) {
    const usize half = 4;
    cflat__radix4butterfly(stride*2, out,        in);
    cflat__radix4butterfly(stride*2, out + half, in + stride);
    const usize e = 0;           
    const usize o = 0 + half;
    
    CflatVec256cf twiddle = cflat_v256cff(
        1, 0,                             //          1 + 0i
        INVSQRT2, -INVSQRT2,              //  1/sqrt(2) - i/sqrt(2)
        0, -1,                            //          0 - i
        -INVSQRT2, -INVSQRT2              // -1/sqrt(2) - i/sqrt(2)
    );

    CflatVec256cf ev       = cflat_load_v256cf(out + e);
    CflatVec256cf od       = cflat_mul_v256cf(cflat_load_v256cf(out + o), twiddle);
    cflat_store_v256cf(out + e, cflat_add_v256cf(ev, od));
    cflat_store_v256cf(out + o, cflat_sub_v256cf(ev, od));
}

static void cflat__radix16butterfly(const usize stride, c32 *restrict out, const c32 *restrict in) {
    const usize fft_size = 16;
    const usize half = 8;

    cflat__radix8butterfly(stride*2, out,        in);
    cflat__radix8butterfly(stride*2, out + half, in + stride);

    for (usize k = 0; k < half; k += cflat_vec_length(CflatVec256cf)) {
        cflat__radix2merge_v256cf(k, fft_size, out);
    }
}

/* ============================================================================================== */
/*                                 Inverse FFT Butterfly                                          */
/* ============================================================================================== */

static void cflat__iradix2merge_v256cf(usize k, usize fft_size, c32 *restrict out) {
    const usize half = fft_size/2;
    const usize e = k;           
    const usize o = k + half;
    CflatVec256cf twiddle = cflat_conj_v256cf(cflat_twiddle_lookup_v256cf(k, fft_size));  // c32 twiddle = conjf(twiddle);
    CflatVec256cf ev = cflat_load_v256cf(out + e);                                        // c32 ev = out[e];
    CflatVec256cf od = cflat_mul_v256cf(cflat_load_v256cf(out + o), twiddle);             // c32 od = out[o] * twiddle;
    cflat_store_v256cf(out + e, cflat_add_v256cf(ev, od));                                // out[e] = ev + od;
    cflat_store_v256cf(out + o, cflat_sub_v256cf(ev, od));                                // out[o] = ev - od;
}

#define cflat__iradix1butterfly clfat__radix1butterfly
#define cflat__iradix2butterfly cflat__radix2butterfly

static void cflat__iradix4butterfly(const usize stride, c32 *restrict out, const c32 *restrict in) {
    c32 x0 = in[0*stride];
    c32 x1 = in[1*stride];
    c32 x2 = in[2*stride];
    c32 x3 = in[3*stride];
    c32 t0 = x0 + x2;
    c32 t1 = x0 - x2;
    c32 t2 = x1 + x3;
    c32 t3 = x1 - x3;
    c32 t3_rot = t3 * I; 
    out[0] = t0 + t2;
    out[1] = t1 + t3_rot;
    out[2] = t0 - t2;
    out[3] = t1 - t3_rot;
}

static void cflat__iradix8butterfly(const usize stride, c32 *restrict out, const c32 *restrict in) {
    const usize fft_size = 8;
    const usize half = 4;
    cflat__iradix4butterfly(stride*2, out,        in);
    cflat__iradix4butterfly(stride*2, out + half, in + stride);
    for (usize k = 0; k < half; k += cflat_vec_length(CflatVec256cf)) {
        cflat__iradix2merge_v256cf(k, fft_size, out);
    }
}

static void cflat__iradix16butterfly(const usize stride, c32 *restrict out, const c32 *restrict in) {
    const usize fft_size = 16;
    const usize half = 8;
    cflat__iradix8butterfly(stride*2, out,        in);
    cflat__iradix8butterfly(stride*2, out + half, in + stride);
    for (usize k = 0; k < half; k += cflat_vec_length(CflatVec256cf)) {
        cflat__iradix2merge_v256cf(k, fft_size, out);
    }
}

/* ============================================================================================== */
/*                                 FFT                                                            */
/* ============================================================================================== */

void cflat__fft_c32(const usize len, const usize stride, c32 *restrict out, const c32 *restrict in) {
    switch (len) {
    case 16: return cflat__radix16butterfly(stride, out, in);
    case  8: return cflat__radix8butterfly(stride,  out, in);
    case  4: return cflat__radix4butterfly(stride,  out, in);
    case  2: return cflat__radix2butterfly(stride,  out, in);
    case  1: return clfat__radix1butterfly(stride,  out, in);
    case  0: return;
    default: break;
    }
    const usize half = len / 2;
    cflat__fft_c32(half, stride*2, out,        in);
    cflat__fft_c32(half, stride*2, out + half, in + stride);
    for (usize k = 0; k < half; k += cflat_vec_length(CflatVec256cf)) {
        cflat__radix2merge_v256cf(k, len, out);
    }
}

void cflat_fft_c32(const usize fft_size, c32 *restrict out, const c32 *restrict in) {
    cflat_assert(cflat_is_pow2(fft_size));
    cflat_assert((uptr)in  % 32 == 0);
    cflat_assert((uptr)out % 32 == 0);
    cflat_twidle_precompute();
    cflat__fft_c32(fft_size, 1, out, in);
}

/* ============================================================================================== */
/*                                 Inverse FFT                                                    */
/* ============================================================================================== */

void cflat__ifft_c32(const usize len, const usize stride, c32 *restrict out, const c32 *restrict in) {
    switch (len) {
    case 16: return cflat__iradix16butterfly(stride, out, in);
    case  8: return cflat__iradix8butterfly(stride,  out, in);
    case  4: return cflat__iradix4butterfly(stride, out, in);
    case  2: return cflat__iradix2butterfly(stride, out, in);
    case  1: return cflat__iradix1butterfly(stride, out, in);
    case  0: return;
    default: break;
    }
    const usize half = len / 2;
    cflat__ifft_c32(half, stride*2, out,        in);
    cflat__ifft_c32(half, stride*2, out + half, in + stride);
    for (usize k = 0; k < half; k += cflat_vec_length(CflatVec256cf)) {
        cflat__iradix2merge_v256cf(k, len, out);
    }
}

void cflat_ifft_c32(const usize fft_size, c32 *restrict out, const c32 *restrict in) {
    cflat_assert(cflat_is_pow2(fft_size));
    cflat_assert((uptr)in  % 32 == 0);
    cflat_assert((uptr)out % 32 == 0);
    cflat_twidle_precompute();
    cflat__ifft_c32(fft_size, 1, out, in);
    const f32 scale = 1.0f/fft_size;
    const CflatVec256f scale_v = cflat_broadcast_v256f(scale);
    usize k = 0;
    cflat_for_vec(CflatVec256cf, k, fft_size)
        cflat_store_v256cf(out + k, cflat_mul_v256cff(cflat_load_v256cf(out + k), scale_v));
    for (; k < fft_size; k += 1)
        out[k] *= scale;
}

#endif // CFLAT_IMPLEMENTATION
#endif //CFLAT_FFT_H