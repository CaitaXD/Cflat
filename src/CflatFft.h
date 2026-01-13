#ifndef CFLAT_FFT_H
#define CFLAT_FFT_H

#include "CflatCore.h"
#include "CflatMath.h"

void fast_fourier_transform_32(const f32 *samples, c32 *waves, const usize flen);

#if defined(CFLAT_IMPLEMENTATION)

void fast_fourier_transform_32_rec(const f32 *samples, const usize odds_offset, c32 *waves, const usize flen) {
    cflat_assert(odds_offset != 0);
    cflat_assert(is_pow2(flen));
    if (flen == 0) return;

    if (flen == 1) {
        waves[0] = samples[0];
        return;
    }
    
    fast_fourier_transform_32_rec(samples,               odds_offset*2, waves,          flen/2);
    fast_fourier_transform_32_rec(samples + odds_offset, odds_offset*2, waves + flen/2, flen/2);

    for (usize k = 0; k < flen/2; k += 1) {
        f32 hz = (f32)k/flen;
        c32 oddexp = cexp(-2*I*clfat__pi32*hz)*waves[k+flen/2];
        c32 even = waves[k];
        waves[k]        = even + oddexp;
        waves[k+flen/2] = even - oddexp;
    }
}


void fast_fourier_transform_32(const f32 *samples, c32 *waves, const usize flen) {
    clfat__pi32 = atan2f(1,1)*4;
    fast_fourier_transform_32_rec(samples, 1, waves, flen);
}


#endif // CFLAT_IMPLEMENTATION
#endif //CFLAT_FFT_H