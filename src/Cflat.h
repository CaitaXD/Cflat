#ifndef CFLAT_CFLAT_H
#define CFLAT_CFLAT_H

#if defined(CFLAT_IMPLEMENTATION)
#   define CFLAT_ARENA_IMPLEMENTATION
#   define CFLAT_ARRAY_IMPLEMENTATION
#   define CFLAT_DA_IMPLEMENTATION
#   define CFLAT_SLICE_IMPLEMENTATION
#endif //CFLAT_IMPLEMENTATION

#if defined(__cplusplus)
#extern "C"
{
#endif

#include "CflatCore.h"
#include "CflatArena.h"
#include "CflatArray.h"
#include "CflatDa.h"
#include "CflatSlice.h"
#include "CflatString.h"


#if defined(__cplusplus)
}
#endif

#endif //CFLAT_CFLAT_H