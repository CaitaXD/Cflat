#ifndef CFLAT_FFT_H
#define CFLAT_FFT_H

#include "CflatCore.h"
#include "CflatMath.h"

void fast_fourier_transform_c32(const usize fft_size, const usize channels, const c32 frames[fft_size][channels], c32 waves[fft_size][channels]);

void inverse_fast_fourier_transform_c32(const usize fft_size, const usize channels, const c32 waves[fft_size][channels], c32 frames[fft_size][channels]);

#if defined(CFLAT_IMPLEMENTATION)

void fast_fourier_transform_c32_rec(const usize len, const usize channels, const usize stride, const c32 in[len][channels], c32 out[len][channels]) {
    cflat_assert(stride != 0);
    cflat_assert(cflat_is_pow2(len));
    if (len == 0) return;

    if (len == 1) {
        for (usize channel = 0; channel < channels; channel += 1) {
            out[0][channel] = in[0][channel];
        }
        return;
    }
    
    fast_fourier_transform_c32_rec(len/2, channels, stride*2, in,          out);
    fast_fourier_transform_c32_rec(len/2, channels, stride*2, in + stride, out + len/2);

    for (usize k = 0; k < len/2; k += 1) {
        f32 bin = (f32)k/len;
        
        c32 exp = -2*I*cflat_pi*bin;
        for (usize channel = 0; channel < channels; channel += 1) {
            c32 odd = cexp(exp) * out[k+len/2][channel];
            c32 even   = out[k][channel];
            out[k][channel]       = even + odd;
            out[k+len/2][channel] = even - odd;
        }
    }
}

void inverse_fast_fourier_transform_c32_rec(const usize len, const usize channels, const usize stride, const c32 in[len][channels], c32 out[len][channels]) {
    cflat_assert(stride != 0);
    cflat_assert(cflat_is_pow2(len));
    if (len == 0) return;

    if (len == 1) {
        for (usize channel = 0; channel < channels; channel += 1) {
            out[0][channel] = in[0][channel];
        }
        return;
    }
    
    inverse_fast_fourier_transform_c32_rec(len/2, channels, stride*2, in,          out);
    inverse_fast_fourier_transform_c32_rec(len/2, channels, stride*2, in + stride, out + len/2);

    for (usize k = 0; k < len/2; k += 1) {
        f32 bin = (f32)k/len;
        
        c32 exp = 2*I*cflat_pi*bin;
        for (usize channel = 0; channel < channels; channel += 1) {
            c32 odd = cexp(exp) * out[k+len/2][channel];
            c32 even   = out[k][channel];
            out[k][channel]       = (even + odd) / 2;
            out[k+len/2][channel] = (even - odd) / 2;
        }
    }
}


void fast_fourier_transform_c32(const usize fft_size, const usize channels, const c32 in[fft_size][channels], c32 out[fft_size][channels]) {
    fast_fourier_transform_c32_rec(fft_size, channels, 1, in, out);    
}

void inverse_fast_fourier_transform_c32(const usize fft_size, const usize channels, const c32 in[fft_size][channels], c32 out[fft_size][channels]) {
    inverse_fast_fourier_transform_c32_rec(fft_size, channels, 1, in, out);
}

#endif // CFLAT_IMPLEMENTATION
#endif //CFLAT_FFT_H