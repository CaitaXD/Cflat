#ifndef CFLAT_CFLAT_STRING_H
#define CFLAT_CFLAT_STRING_H

#include "CflatDa.h"
#include "CflatArray.h"
#include "CflatSlice.h"

typedef cflat_define_da(char, cflat_string_builder) CflatStringBuilder;
typedef cflat_define_array(char, cflat_string) CflatString;
typedef cflat_define_slice(char, cflat_string_view) CflatStringView;

#define cflat__padded_string(STR)                                                                                      \
    cflat_padded_struct(CflatString, sizeof(CflatString)+sizeof(STR), .length=sizeof((STR))-1)                         \

#define cflat__padded_string_builder(STR, a)                                                                           \
    cflat_padded_struct(CflatStringBuilder,                                                                            \
        align_pow2(sizeof(CflatStringBuilder)+sizeof(STR), 16),                                                        \
        .arena = (a),                                                                                                  \
        .capacity=align_pow2(sizeof(CflatStringBuilder)+sizeof(STR), 16),                                              \
        .count=sizeof((STR))-1                                                                                         \
    )                                                                                                                  \

#define cflat_reinterpret_cast(To, X) (*(To*)&(X))

#define cflat_generic_match_pointer(T, value)                                                                          \
    T*: (value),                                                                                                       \
    T const*: (value),                                                                                                 \
    const T*: (value),                                                                                                 \
    const T const*: (value)                                                                                            \

#define cflat_sv_lit(S) _Generic(((cflat_typeof((S))*)0),                                                              \
                                                                                                                       \
    char(*)[sizeof(S)]: (CflatStringView) { /*NOLINT(*-sizeof-expression)*/                                            \
        cflat_reinterpret_cast(cflat_typeof(char[sizeof(S)]), (S)), /*NOLINT(*-sizeof-expression)*/                    \
        sizeof((S))-1                                                                                                  \
    },                                                                                                                 \
                                                                                                                       \
    cflat_generic_match_pointer(char*, ((CflatStringView) {                                                            \
        cflat_reinterpret_cast(char*,(S)),                                                                             \
        strlen(cflat_reinterpret_cast(char*,(S)))                                                                      \
    })),                                                                                                               \
                                                                                                                       \
    cflat_generic_match_pointer(CflatString*, ((CflatStringView) {                                                     \
        cflat_reinterpret_cast(CflatString*,(S))->data,                                                                \
        cflat_reinterpret_cast(CflatString*,(S))->length                                                               \
    })),                                                                                                               \
                                                                                                                       \
    cflat_generic_match_pointer(CflatStringBuilder*, ((CflatStringView) {                                              \
        cflat_reinterpret_cast(CflatStringBuilder*,(S))->data,                                                         \
        cflat_reinterpret_cast(CflatStringBuilder*,(S))->count                                                         \
    })),                                                                                                               \
                                                                                                                       \
    CflatStringView*: (S)                                                                                              \
)

#define STR_LIT(STR)                                                                                                   \
    container_of(                                                                                                      \
        cflat_mem_copy(cflat__padded_string(STR).data, (STR), sizeof(STR)),                                            \
        CflatString,                                                                                                   \
        data                                                                                                           \
    )

#define SB_FROM_LIT(STR, a)                                                                                            \
    container_of(                                                                                                      \
        cflat_mem_copy(cflat__padded_string_builder((STR), a).data, (STR), sizeof(STR)),                               \
        CflatStringBuilder,                                                                                            \
        data                                                                                                           \
    )

#if !defined(CFLAT_STRING_NO_ALIAS)
#   define StringBuilder CflatStringBuilder
#   define String CflatString
#   define StringView CflatStringView
#   define sv_lit cflat_sv_lit
#endif // CFLAT_STRING_NO_ALIAS


#endif //CFLAT_CFLAT_STRING_H
