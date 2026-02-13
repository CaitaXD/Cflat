#ifndef CFLAT_LINEAR_H
#define CFLAT_LINEAR_H

#include "CflatCore.h"
#include "CflatMath.h"

c32* cflat_mat_transpose_c32(usize rows, usize cols, c32 (*restrict out)[cols][rows], const c32 (*restrict in)[rows][cols]);

#if defined(CFLAT_IMPLEMENTATION)

c32* cflat_mat_transpose_c32(usize rows, usize cols, c32 (*restrict out)[cols][rows], const c32 (*restrict in)[rows][cols]) {
    for (usize i = 0; i < rows; ++i)
    for (usize j = 0; j < cols; ++j) {
        (*out)[j][i] = (*in)[i][j];
    }
    return (void*)out;
}

#endif // CFLAT_IMPLEMENTATION

#if !defined(CFLAT_LINEAR_NO_ALIAS)

#define mat_transpose_c32 cflat_mat_transpose_c32

#endif // CFLAT_LINEAR_NO_ALIAS

#endif //CFLAT_LINEAR_H