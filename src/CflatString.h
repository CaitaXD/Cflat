#ifndef CFLAT_CFLAT_STRING_H
#define CFLAT_CFLAT_STRING_H

#include "CflatArena.h"
#include "CflatCore.h"
#include "CflatDa.h"
#include "CflatSlice.h"
#include <limits.h>
#include <stddef.h>

typedef struct cflat_string_view CflatStringView;

CFLAT_DEF CflatStringView cflat_sv_from_cstr           (const char *cstr                                                   );
CFLAT_DEF CflatStringView cflat_sv_from_sv             (CflatStringView sv                                                 );
CFLAT_DEF CflatStringView cflat_sv_clone_cstr          (CflatArena *a, const char *cstr                                    );
CFLAT_DEF CflatStringView cflat_sv_clone_sv            (CflatArena *a, CflatStringView sv                                  );
CFLAT_DEF CflatStringView cflat_sv_find_substring_cstr (CflatStringView strnig, const char         *substring              );
CFLAT_DEF CflatStringView cflat_sv_find_substring_sv   (CflatStringView strnig, CflatStringView     substring              );
CFLAT_DEF isize           cflat_sv_find_index_cstr     (CflatStringView strnig, const char *substring                      );
CFLAT_DEF isize           cflat_sv_find_index_sv       (CflatStringView strnig, CflatStringView substring                  );
CFLAT_DEF isize           cflat_sv_find_last_index_cstr(CflatStringView strnig, const char *substring                      );
CFLAT_DEF isize           cflat_sv_find_last_index_sv  (CflatStringView strnig, CflatStringView substring                  );
CFLAT_DEF CflatStringView cflat_path_name_cstr         (const char *path                                                   );
CFLAT_DEF CflatStringView cflat_path_name_sv           (CflatStringView path                                               );

#define CFLAT__STRING_OVERLOAD(STR, func) _Generic((STR)                            \
    , char*:                        func##_cstr                                     \
    , const char*:                  func##_cstr                                     \
    , CflatStringView:              func##_sv                                       \
)

#define cflat_sv_lit(STR)                             (CflatStringView) { .data = (STR), .length = sizeof(STR) - 1, .capacity = sizeof(STR) }
#define cflat_sv_clone(ARENA, STR)                    CFLAT__STRING_OVERLOAD((STR), cflat_sv_clone)((ARENA), (STR))
#define cflat_sv_find_substring(strnig, substring)    CFLAT__STRING_OVERLOAD((substring), cflat_sv_find_substring)((strnig), (substring))
#define cflat_sv_find_index(strnig, substring)        CFLAT__STRING_OVERLOAD((substring), cflat_sv_find_index)((strnig), (substring))
#define cflat_sv_find_last_index(strnig, substring)   CFLAT__STRING_OVERLOAD((substring), cflat_sv_find_last_index)((strnig), (substring))
#define cflat_path_name(path)                         CFLAT__STRING_OVERLOAD((path), cflat_path_name)((path))
#define cflat_sv_is_nullterm(SV)                      ((SV).capacity > (SV).length && (SV).data[(SV).length] == '\0')

#if defined(CFLAT_IMPLEMENTATION)

struct cflat_string_view {
    CFLAT_SLICE_FIELDS(char);
};

CflatStringView cflat_sv_from_cstr(const char *cstr)  { 
    const usize len = strlen(cstr);
    return (CflatStringView) { 
        .data     = (void*)cstr, 
        .length   = len, 
        .capacity = len + 1
    };
 }

CflatStringView cflat_sv_clone_cstr(CflatArena *a, const char *cstr) {
    const usize len = strlen(cstr);
    const usize cap = len + 1;
    CflatStringView result = (CflatStringView) {
        .data = arena_push(a, cap, .align = cflat_alignof(char), .clear = false),
        .length = len,
        .capacity = cap,
    };
    cflat_mem_copy(result.data, cstr, len);
    result.data[len] = '\0';
    return result;
}

CflatStringView cflat_sv_clone_sv(CflatArena *a, CflatStringView sv) {
    const usize cap = sv.length + 1;
    CflatStringView result = (CflatStringView) {
        .data = arena_push(a, sizeof(char)*cap, .align = cflat_alignof(char), .clear = false),
        .length = sv.length,
        .capacity = cap,
    };
    cflat_mem_copy(result.data, sv.data, sv.length);
    result.data[sv.length] = '\0';
    return result;
}

typedef struct cflat_matrix {
    usize rows;
    usize columns;
    byte data[];
} CflatMatrix;

CflatMatrix* (cflat__dfa_new)(CflatArena *a, CflatStringView pattern) {

    byte (*transitions)[pattern.length + 1][UCHAR_MAX + 1]; 
    
    CflatMatrix *dfa = arena_push(a, sizeof(CflatMatrix) + sizeof(*transitions), 0);
    dfa->rows    = CFLAT_ROW_SIZE(*transitions);
    dfa->columns = CFLAT_COL_SIZE(*transitions);
    transitions = (void*)dfa->data;

    for (usize i = 0; i < dfa->columns; ++i) (*transitions)[0][i] = 0;
    (*transitions)[0][(u8)pattern.data[0]] = 1;
    
    usize state = 0;

    for (usize q = 1; q <= pattern.length; ++q) {
        for (usize i = 0; i < dfa->columns; ++i) {
            (*transitions)[q][i] = (*transitions)[state][i];
        }
        if (q < pattern.length) (*transitions)[q][(u8)pattern.data[q]] = q + 1;
        state = (*transitions)[state][(u8)pattern.data[q]];
    }
    
    return dfa;
}

isize cflat_sv_find_index_sv(CflatStringView string, CflatStringView substring) {
    usize index = -1;
    CflatTempArena scratch;
    arena_scratch_scope(scratch, 0) {
        CflatMatrix *dfa = cflat__dfa_new(scratch.arena, substring);
        byte (*transitions)[dfa->rows][dfa->columns] = (void*)dfa->data;
        
        usize state = 0;
        for (usize i = 0; i < string.length; ++i) {
            state = (*transitions)[state][(u8)string.data[i]];
            if (state == substring.length) {
            
                index = i - substring.length + 1;
                scope_exit;
            }
        }
    }

    return index;
}

isize cflat_sv_find_last_index_sv(CflatStringView string, CflatStringView substring) {
    usize index = -1;
    CflatTempArena scratch;
    arena_scratch_scope(scratch, 0) {
        CflatMatrix *dfa = cflat__dfa_new(scratch.arena, substring);
        byte (*transitions)[dfa->rows][dfa->columns] = (void*)dfa->data;
        
        usize state = 0;
        for (usize i = 0; i < string.length; ++i) {
            state = (*transitions)[state][(u8)string.data[i]];
            if (state == substring.length) {
                index = i - substring.length + 1;
            }
        }
    }
    return index;
}

isize cflat_sv_find_index_cstr(CflatStringView string, const char *substring) {
     return cflat_sv_find_index_sv(string, cflat_sv_from_cstr(substring)); 
}

isize cflat_sv_find_last_index_cstr(CflatStringView string, const char *substring) { 
    return cflat_sv_find_last_index_sv(string, cflat_sv_from_cstr(substring));
}

CflatStringView cflat_sv_find_substring_sv(CflatStringView string, CflatStringView substring) {
    isize index = cflat_sv_find_index_sv(string, substring);
    if (index < 0) return (CflatStringView){0};
    return (CflatStringView) {
        .data = string.data + index,
        .length = string.length - index,
        .capacity = string.capacity - index,
    };
}

CflatStringView cflat_sv_find_substring_cstr(CflatStringView string, const char *substring) {
    isize index = cflat_sv_find_index_sv(string, cflat_sv_from_cstr(substring));
    if (index < 0) return (CflatStringView){0};
    return (CflatStringView) {
        .data = string.data + index,
        .length = string.length - index,
        .capacity = string.capacity - index,
    };
}

CflatStringView cflat_path_name_sv(CflatStringView path) {
#if defined(OS_WINDOWS)
    CflatStringView p1 = cflat_sv_find_substring_sv(path, (CflatStringView) { .data = "/" ,  .length = 1 });
    CflatStringView p2 = cflat_sv_find_substring_sv(path, (CflatStringView) { .data = "\\", .length  = 1 });
    CflatStringView p = (p1.data > p2.data) ? p1 : p2;
    return (CflatStringView) {
        .data   = p.data ? p.data   + 1 : path.data,
        .length = p.data ? p.length - 1 : path.length,
        .capacity = p.data ? p.capacity - 1 : path.capacity,
    };
#else
    CflatStringView p = cflat_find_substring_sv(path, (CflatStringView) { .data = "/" ,  .length = 1 });
    return (CflatStringView) {
        .data   = p.data ? p.data   + 1 : path.data,
        .length = p.data ? p.length - 1 : path.length,
        .capacity = p.data ? p.capacity - 1 : path.capacity,
    };
#endif // _WIN32
}

CflatStringView cflat_path_name_cstr (const char *path)         { return cflat_path_name_sv(cflat_sv_from_cstr(path)); }

#endif // CFLAT_IMPLEMENTATION


#if !defined(CFLAT_STRING_NO_ALIAS)
#   define sv_is_nullterm cflat_sv_is_nullterm
#   define sv_lit cflat_sv_lit
#   define sv_lit cflat_sv_lit
#   define sv_from_cstr cflat_sv_from_cstr
#   define sv_clone cflat_sv_clone
#   define sv_find_substring cflat_sv_find_substring
#   define sv_find_index cflat_sv_find_index
#   define path_name cflat_path_name
#   define sv_find_last_index cflat_sv_find_last_index
#   define StringBuilder CflatStringBuilder
#   define String CflatString
#   define StringView CflatStringView
#   define Path CflatPath
#endif // CFLAT_STRING_NO_ALIAS


#endif //CFLAT_CFLAT_STRING_H
