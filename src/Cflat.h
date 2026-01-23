/*
    Cflat is a collection of C data structures and algorithms.

    Conventions:
        - All library functions must be prefixed with cflat_
        - Internal functions must be prefixed with cflat__
        - All must library functions have an alias without the cflat_ prefix, which can be disabled with by defining CFLAT_NO_ALIAS to 1

        - Types of constructors:
            - cflat_xxx_new: Allocates memory and initializes the object
                Takes in Arena *a as first argument (second int the case whhen a function takes in element_size)
            
            - cflat_xxx_init: Initializes
                Should never allocate memory
            
            - cflat_xxx_lit: Creates an object using a compund literal
                Should be able to be used in static initialization

        - Types of destructors:
            - cflat_xxx_delete: Frees memory
            - cflat_xxx_clear: Makes object ready for reuse should never free memory
        
        - Scope macros:
            Macros of the form cflat_xxx_scope, will be inplemented as a for loop and must be used with a block scope
            ```c
            TempArena temp;
            cflat_scratch_arena_scope(temp) { // gets a temporary arena
                // Code
            } // resets the temporary arena
            ```
*/
#ifndef CFLAT_CFLAT_H
#define CFLAT_CFLAT_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "CflatCore.h"
#include "CflatArena.h"
#include "CflatArray.h"
#include "CflatDa.h"
#include "CflatSlice.h"
#include "CflatString.h"
#include "CflatMath.h"
#include "CflatHashSet.h"

#if defined(__cplusplus)
}
#endif

#endif //CFLAT_CFLAT_H
