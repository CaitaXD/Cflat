#ifndef CFLAT_FFT_H
#define CFLAT_FFT_H

#include "CflatArena.h"
#include "CflatCore.h"
#include "CflatMath.h"
#include <complex.h>


void fast_fourier_transform_c32(const usize fft_size, const usize channels, const c32 *restrict in, c32 *restrict out);

void inverse_fast_fourier_transform_c32(const usize fft_size, const usize channels, const c32 *restrict in, c32 *restrict out);

#if defined(CFLAT_IMPLEMENTATION)

#define SQRT2_OVER_2 (0.7071067811865475244008443621048490392848359376884740365883398689)

static void __radix2merge(usize k, c32 twiddle, usize channel, usize fft_size, usize channels, c32 *restrict out) {
    const usize half = fft_size / 2;
    const usize e = (k) * channels + channel;           
    const usize o = (k + half) * channels + channel;  
    c32 ev = out[e];                        
    c32 od = out[o] * (twiddle);                 
    out[e] = ev + od;                       
    out[o] = ev - od;                       
}

static void __radix2merge_scaled(usize k, c32 twiddle, usize channel, usize fft_size, usize channels, c32 *restrict out) {
    const usize half = fft_size / 2;
    const usize e = (k) * channels + channel;           
    const usize o = (k + half) * channels + channel;  
    c32 ev = out[e];                        
    c32 od = out[o] * (twiddle);                 
    out[e] = (ev + od)/2;                       
    out[o] = (ev - od)/2;                           
}

static void __radix1butterfly(const usize channels, const usize stride, const c32 *restrict in, c32 *restrict out) {
    for (usize c = 0; c < channels; c++) {
        out[0 * channels + c] = in[0 * stride * channels + c];
    }
}

static void __radix2butterfly(const usize channels, const usize stride, const c32 *restrict in, c32 *restrict out) {
    for (usize c = 0; c < channels; c++) {
        c32 a = in[0 * stride * channels + c];
        c32 b = in[1 * stride * channels + c];
        out[0 * channels + c] = a + b;
        out[1 * channels + c] = a - b;
    }
}

static void __radix4butterfly(const usize channels, const usize stride, const c32 *restrict in, c32 *restrict out) {
    const usize s1 = 1 * stride * channels;
    const usize s2 = 2 * stride * channels;
    const usize s3 = 3 * stride * channels;

    for (usize c = 0; c < channels; c++) {
        c32 x0 = in[0 + c];
        c32 x1 = in[s1 + c];
        c32 x2 = in[s2 + c];
        c32 x3 = in[s3 + c];

        c32 t0 = x0 + x2;
        c32 t1 = x0 - x2;
        c32 t2 = x1 + x3;
        c32 t3 = x1 - x3;

        c32 t3_rot = t3 * -I;

        out[0 * channels + c] = t0 + t2;
        out[1 * channels + c] = t1 + t3_rot;
        out[2 * channels + c] = t0 - t2;
        out[3 * channels + c] = t1 - t3_rot;
    }
}


static void  __radix8butterfly(const usize channels, const usize stride, const c32 *restrict in, c32 *restrict out) {
    const usize s1 = 1 * stride * channels;
    const usize s2 = 2 * stride * channels;
    const usize s3 = 3 * stride * channels;
    const usize s4 = 4 * stride * channels;
    const usize s5 = 5 * stride * channels;
    const usize s6 = 6 * stride * channels;
    const usize s7 = 7 * stride * channels;

    for (usize c = 0; c < channels; c++) {
        c32 x0 = in[0 + c];  c32 x1 = in[s1 + c];
        c32 x2 = in[s2 + c]; c32 x3 = in[s3 + c];
        c32 x4 = in[s4 + c]; c32 x5 = in[s5 + c];
        c32 x6 = in[s6 + c]; c32 x7 = in[s7 + c];

        c32 a0 = x0 + x4; c32 a4 = x0 - x4;
        c32 a1 = x1 + x5; c32 a5 = x1 - x5;
        c32 a2 = x2 + x6; c32 a6 = x2 - x6;
        c32 a3 = x3 + x7; c32 a7 = x3 - x7;

        c32 b0 = a0 + a2; c32 b2 = a0 - a2;
        c32 b1 = a1 + a3; c32 b3 = a1 - a3;
        c32 b4 = a4 + (a6 * -I);
        c32 b6 = a4 - (a6 * -I);
        
        c32 r45  = ( SQRT2_OVER_2 + I * -SQRT2_OVER_2);
        c32 r135 = (-SQRT2_OVER_2 + I * -SQRT2_OVER_2);
        c32 b5 = a5 * r45;
        c32 b7 = a7 * r135;

        out[0 * channels + c] = b0 + b1;
        out[4 * channels + c] = b0 - b1;
        out[2 * channels + c] = b2 + (b3 * -I);
        out[6 * channels + c] = b2 - (b3 * -I);
        out[1 * channels + c] = b4 + b5;
        out[5 * channels + c] = b4 - b5;
        out[3 * channels + c] = b6 + b7;
        out[7 * channels + c] = b6 - b7;
    }
}

static void __radix16butterfly(const usize channels, const usize stride, const c32 *restrict in, c32 *restrict out, const c32 *restrict twiddles, const usize t_stride) {
    const usize fft_size = 16;
    const usize half = 8;

    __radix8butterfly(channels, stride * 2, in, out);
    __radix8butterfly(channels, stride * 2, in + (stride * channels), out + (half * channels));

    for (usize k = 0; k < half; k++) {
        c32 twiddle = twiddles[k*t_stride];
        for (usize c = 0; c < channels; c++) {
            __radix2merge(k, twiddle, c, fft_size, channels, out);
        }
    }
}

void __ifft_lut_c32(const usize fft_size, const usize channels, const usize stride, const c32 *restrict in, c32 *restrict out, const c32 *restrict twiddles, const usize t_stride) {
    cflat_assert(stride != 0);
    cflat_assert(cflat_is_pow2(fft_size));

    switch (fft_size) {
    case  2: return __radix2butterfly(channels, stride, in, out);
    case  1: return __radix1butterfly(channels, stride, in, out);
    case  0: return;
    default: break;
    }

    const usize half = fft_size / 2;
    const usize step = stride * 2;

    __ifft_lut_c32(half, channels, step, in, out, twiddles, t_stride*2);
    __ifft_lut_c32(half, channels, step, in + (stride * channels), out + (half * channels), twiddles, t_stride*2);

    for (usize k = 0; k < half; k++) {
        c32 twiddle = conjf(twiddles[k*t_stride]);
        for (usize c = 0; c < channels; c++) {
            __radix2merge_scaled(k, twiddle, c, fft_size, channels, out);
        }
    }
}

#if defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#   define cflat_unlikely(x) __builtin_expect(!!(x), 0)
#   define cflat_likely(x)   __builtin_expect(!!(x), 1)
#else
#   define cflat_unlikely(x) (x)
#   define cflat_likely(x)   (x)
#endif

cflat_thread_local static usize tls_twiddle_size = 0;
_Alignas(64) cflat_thread_local static c32 tls_twiddles[KiB(64)];

static void precompute_twiddles(const usize fft_size) {
    if (cflat_likely(tls_twiddle_size == fft_size)) return;
    for (usize i = 0; i < fft_size/2; i += 1) tls_twiddles[i] = cexpf(-2.0f*I*cflat_pi*(f32)i/(f32)fft_size);
    tls_twiddle_size = fft_size;
}

void __fft_c32(const usize len, const usize channels, const usize stride, const c32 *restrict in, c32 *restrict out) {
    cflat_assert(stride != 0);
    cflat_assert(cflat_is_pow2(len));

    switch (len) {
    case  1: return __radix1butterfly(channels, stride, in, out);
    case  0: return;
    default: break;
    }

    const usize half = len / 2;
    const usize step = stride * 2;

    __fft_c32(half, channels, step, in, out);
    __fft_c32(half, channels, step, in + (stride * channels), out + (half * channels));

    for (usize k = 0; k < half; k++) {
        c32 twiddle = cexpf(-2.0f*I*cflat_pi*(f32)k/(f32)len);
        for (usize c = 0; c < channels; c++) {
            usize even_idx = k * channels + c;
            usize odd_idx  = (k+half) * channels + c;

            c32 even = out[even_idx];
            c32 odd  = out[odd_idx] * twiddle;

            out[even_idx] = (even + odd);
            out[odd_idx]  = (even - odd);
        }
    }
}

void __ifft_c32(const usize len, const usize channels, const usize stride, const c32 *restrict in, c32 *restrict out) {
    cflat_assert(stride != 0);
    cflat_assert(cflat_is_pow2(len));

    switch (len) {
    case  1: return __radix1butterfly(channels, stride, in, out);
    case  0: return;
    default: break;
    }

    const usize half = len / 2;
    const usize step = stride * 2;

    __ifft_c32(half, channels, step, in, out);
    __ifft_c32(half, channels, step, in + (stride * channels), out + (half * channels));

    for (usize k = 0; k < half; k++) {
        c32 twiddle = cexpf(2.0f*I*cflat_pi*(f32)k/(f32)len);
        for (usize c = 0; c < channels; c++) {
            usize even_idx = k * channels + c;
            usize odd_idx  = (k+half) * channels + c;

            c32 even = out[even_idx];
            c32 odd  = out[odd_idx] * twiddle;

            out[even_idx] = (even + odd)/2;
            out[odd_idx]  = (even - odd)/2;
        }
    }
}


void __fft_lut_c32(const usize fft_size, const usize channels, const usize stride, const c32 *restrict in, c32 *restrict out, const c32 *restrict twiddles, const usize t_stride) {
    cflat_assert(stride != 0);
    cflat_assert(cflat_is_pow2(fft_size));

    switch (fft_size) {
    case 16: return __radix16butterfly(channels, stride, in, out, twiddles, t_stride);
    case  8: return __radix8butterfly(channels, stride, in, out);
    case  4: return __radix4butterfly(channels, stride, in, out);
    case  2: return __radix2butterfly(channels, stride, in, out);
    case  1: return __radix1butterfly(channels, stride, in, out);
    case  0: return;
    default: break;
    }

    const usize half = fft_size / 2;
    const usize step = stride * 2;

    __fft_lut_c32(half, channels, step, in, out, twiddles, t_stride*2);
    __fft_lut_c32(half, channels, step, in + (stride * channels), out + (half * channels), twiddles, t_stride*2);

    for (usize k = 0; k < half; k++) {
        c32 twiddle = twiddles[k*t_stride];
        for (usize c = 0; c < channels; c++) {
            __radix2merge(k, twiddle, c, fft_size, channels, out);
        }
    }
}

void fast_fourier_transform_c32(const usize fft_size, const usize channels, const c32 *restrict in, c32 *restrict out) {
    cflat_assert(cflat_is_pow2(fft_size));
    cflat_assert(fft_size <= CFLAT_ARRAY_SIZE(tls_twiddles) && "FFT size too large");
    precompute_twiddles(fft_size);
    __fft_lut_c32(fft_size, channels, 1, in, out, tls_twiddles, 1);    
}

void inverse_fast_fourier_transform_c32(const usize fft_size, const usize channels, const c32 *restrict in, c32 *restrict out) {
    cflat_assert(cflat_is_pow2(fft_size));
    cflat_assert(fft_size <= CFLAT_ARRAY_SIZE(tls_twiddles) && "FFT size too large");
    precompute_twiddles(fft_size);
    __ifft_lut_c32(fft_size, channels, 1, in, out, tls_twiddles, 1);
}

#endif // CFLAT_IMPLEMENTATION
#endif //CFLAT_FFT_H