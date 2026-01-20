#ifndef CFATLATMATH_H
#define CFATLATMATH_H

#include <math.h>
#include <complex.h>
#include "CflatCore.h"

// Doing 2 typedefs cause clangd was shitting itsself with the complex keyword
typedef float complex complex_float;
typedef double complex complex_double;
typedef long double complex complex_long_double;

#if __SIZEOF_FLOAT__ == 4
typedef complex_float c32;
#endif
#if __SIZEOF_DOUBLE__ == 8
typedef complex_double c64;
#endif
#if __SIZEOF_LONG_DOUBLE__ == 16
typedef complex_long_double c128;
#endif

CFLAT_DEF f32 ilerp_f32(f32 a, f32 b, f32 t);
CFLAT_DEF f32 lerp_f32(f32 a, f32 b, f32 t);
CFLAT_DEF f32 expdecay_f32(f32 a, f32 b, f32 decay, f32 dt);
CFLAT_DEF f32 remap_f32(f32 start1, f32 stop1, f32 start2, f32 stop2, float value);
CFLAT_DEF f32 clamp_f32(f32 x, f32 min, f32 max);

CFLAT_DEF c32 lerp_c32(c32 a, c32 b, f32 t);
CFLAT_DEF f32 dot_c32(c32 a, c32 b);
CFLAT_DEF c32 norm_c32(c32 a);
CFLAT_DEF c32 rotate_c32(c32 a, f32 theta);
CFLAT_DEF c32 slerp_c32(c32 a, c32 b, f32 t);
CFLAT_DEF c32 sexpdecay_c32(c32 a, c32 b, f32 decay, f32 dt);

#if !defined(CFLAT_NO_GENERIC_MATH)

#if __SIZEOF_FLOAT__ == 4
const f32 clfat_pi32 = 3.14159265358979323846264338327950288419716939937510582097494459230781640628620899;
#endif
#if __SIZEOF_DOUBLE__ == 8
const f64 clfat_pi64 = 3.14159265358979323846264338327950288419716939937510582097494459230781640628620899;
#endif
#if __SIZEOF_LONG_DOUBLE__ == 16
const f128 clfat_pi128 = 3.14159265358979323846264338327950288419716939937510582097494459230781640628620899;
#endif

#define log_base(base, x) (log((x))/log((base)))

#define lerp(A,B,T) _Generic((A),                           \
    f32: lerp_f32((A), (B), (T)),                           \
    c32: lerp_c32((A), (B), (T))                            \
)

#define ilerp(A,B,T) _Generic((A),                           \
    f32: ilerp_f32((A), (B), (T))                            \
)

#define sin(X) _Generic((X),                                \
    float: sinf((X)),                                       \
    double: sin((X)),                                       \
    long double: sinl((X)),                                 \
    float complex: csinf((X)),                              \
    double complex: csin((X)),                              \
    long double complex: csinl((X))                         \
)

#define cos(X) _Generic((X),                                \
    float: cosf((X)),                                       \
    double: cos((X)),                                       \
    long double: cosl((X)),                                 \
    float complex: ccosf((X)),                              \
    double complex: ccos((X)),                              \
    long double complex: ccosl((X))                         \
)

#define exp(X) _Generic((X),                                \
    float: expf((X)),                                       \
    double: exp((X)),                                       \
    long double: expl((X)),                                 \
    float complex: cexpf((X)),                              \
    double complex: cexp((X)),                              \
    long double complex: cexpl((X))                         \
)

#define creal(X) _Generic((X),                              \
    float complex: crealf((X)),                             \
    double complex: creal((X)),                             \
    long double complex: creall((X))                        \
)

#define cimag(X) _Generic((X),                              \
    float complex: cimagf((X)),                             \
    double complex: cimag((X)),                             \
    long double complex: cimagl((X))                        \
)

#endif // CFLAT_NO_GENERIC_MATH

#if defined(CFLAT_IMPLEMENTATION)

f32 ilerp_f32(f32 a, f32 b, f32 t) {
    return (t - a) / (b - a);
}

f32 lerp_f32(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

c32 lerp_c32(c32 a, c32 b, f32 t) {
    return a + (b - a) * t;
}

f32 expdecay_f32(f32 a, f32 b, f32 decay, f32 dt) {
    return b + (a - b) * expf(-decay*dt);
}

f32 dot_c32(c32 a, c32 b) {
    return crealf(a * conjf(b));
}

f32 clamp_f32(f32 x, f32 min, f32 max) {
    return fmaxf(min, fminf(x, max));
}

c32 norm_c32(c32 a) {
    return a / cabsf(a);
}

c32 rotate_c32(c32 a, f32 theta) {
    return cexpf(I * theta) * a;
}

c32 slerp_c32(c32 a, c32 b, f32 t) {    
    f32 a_sqmag = dot_c32(a, a);
    f32 b_sqmag = dot_c32(b, b);

    if (a_sqmag == 0 || b_sqmag == 0) {
        return lerp_c32(a, b, t);
    }

    f32 start_len = sqrtf(a_sqmag);
    f32 end_len = lerp_f32(start_len, sqrtf(b_sqmag), t);
    f32 theta = cargf(a) - cargf(b);

    return rotate_c32(a, theta*t) * (end_len / start_len);
}

c32 sexpdecay_c32(c32  a, c32 b, f32 decay, f32 dt) {
    return slerp_c32(b, a, expf(-decay*dt));
}

f32 remap_f32(f32 start1, f32 stop1, f32 start2, f32 stop2, float value) {
    return start2 + (stop2 - start2) * ((value - start1) / (stop1 - start1));
}

#endif // CFLAT_IMPLEMENTATION

#endif //CFATLATMATH_H