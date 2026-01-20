#ifndef CFLAT_FFT_H
#define CFLAT_FFT_H

#include "CflatCore.h"
#include "CflatMath.h"

void fast_fourier_transform_f32(const usize fft_size, const usize channels, const f32 frames[fft_size][channels], c32 waves[fft_size][channels]);

#if defined(CFLAT_IMPLEMENTATION)

void fast_fourier_transform_32_rec(const usize len, const usize channels, const usize stride, const f32 in[len][channels], c32 out[len][channels]) {
    cflat_assert(stride != 0);
    cflat_assert(is_pow2(len));
    if (len == 0) return;

    if (len == 1) {
        
        for (usize channel = 0; channel < channels; channel += 1) {
            out[0][channel] = in[0][channel];
        }
        return;
    }
    
    fast_fourier_transform_32_rec(len/2, channels, stride*2, in,          out);
    fast_fourier_transform_32_rec(len/2, channels, stride*2, in + stride, out + len/2);

    for (usize k = 0; k < len/2; k += 1) {
        f32 bin = (f32)k/len;
        
        for (usize channel = 0; channel < channels; channel += 1) {
            c32 oddexp = cexp(-2*I*clfat_pi32*bin) * out[k+len/2][channel];
            c32 even   = out[k][channel];
            out[k][channel]       = even + oddexp;
            out[k+len/2][channel] = even - oddexp;
        }
    }
}

void fast_fourier_transform_f32(const usize fft_size, const usize channels, const f32 frames[fft_size][channels], c32 out[fft_size][channels]) {
    fast_fourier_transform_32_rec(fft_size, channels, 1, frames, out);    
}


#endif // CFLAT_IMPLEMENTATION
#endif //CFLAT_FFT_H