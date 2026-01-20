#ifndef CFLAT_CFLAT_STRING_H
#define CFLAT_CFLAT_STRING_H

#include "CflatArena.h"
#include "CflatCore.h"
#include "CflatDa.h"
#include "CflatArray.h"
#include "CflatSlice.h"
#include <limits.h>
#include <stddef.h>

typedef cflat_define_da   (char, cflat_string_builder) CflatStringBuilder;
typedef cflat_define_array(char, cflat_string        )  CflatString;
typedef cflat_define_slice(char, cflat_string_view   )  CflatStringView;

#define CFLAT__STRING_OVERLOAD(str, func) _Generic((str)                            \
    , char*:                        func##_cstr                                     \
    , const char*:                  func##_cstr                                     \
    , CflatString*:                 func##_str                                      \
    , const CflatString*:           func##_str                                      \
    , CflatStringBuilder*:          func##_sb                                       \
    , const CflatStringBuilder*:    func##_sb                                       \
    , CflatStringView:              func##_sv                                       \
)

CFLAT_DEF CflatStringView cflat_sv_from_cstr(const char         *cstr);
CFLAT_DEF CflatStringView cflat_sv_from_str (CflatString        *str );
CFLAT_DEF CflatStringView cflat_sv_from_sb  (CflatStringBuilder *sb  );
CFLAT_DEF CflatStringView cflat_sv_from_sv  (CflatStringView     sv  );

#define cflat_sv_from(str) CFLAT__STRING_OVERLOAD((str), cflat_sv_from)((str))

CFLAT_DEF CflatString *cflat_str_clone_cstr(CflatArena *a, const char         *cstr);
CFLAT_DEF CflatString *cflat_str_clone_str (CflatArena *a, CflatString        *str );
CFLAT_DEF CflatString *cflat_str_clone_sb  (CflatArena *a, CflatStringBuilder *sb  );
CFLAT_DEF CflatString *cflat_str_clone_sv  (CflatArena *a, CflatStringView     sv  );

#define cflat_str_clone(a, str) CFLAT__STRING_OVERLOAD((str), cflat_str_clone)((a), (str))

CFLAT_DEF CflatStringView cflat_sv_find_substring_cstr(CflatStringView strnig, const char         *substring);
CFLAT_DEF CflatStringView cflat_sv_find_substring_str (CflatStringView strnig, CflatString        *substring);
CFLAT_DEF CflatStringView cflat_sv_find_substring_sb  (CflatStringView strnig, CflatStringBuilder *substring);
CFLAT_DEF CflatStringView cflat_sv_find_substring_sv  (CflatStringView strnig, CflatStringView     substring);

#define cflat_sv_find_substring(strnig, substring) CFLAT__STRING_OVERLOAD((substring), cflat_sv_find_substring)((strnig), (substring))

CFLAT_DEF isize cflat_sv_find_index_cstr(CflatStringView strnig, const char *substring        );
CFLAT_DEF isize cflat_sv_find_index_str (CflatStringView strnig, CflatString *substring       );
CFLAT_DEF isize cflat_sv_find_index_sv  (CflatStringView strnig, CflatStringView substring    );
CFLAT_DEF isize cflat_sv_find_index_sb  (CflatStringView strnig, CflatStringBuilder *substring);

#define cflat_sv_find_index(strnig, substring) CFLAT__STRING_OVERLOAD((substring), cflat_sv_find_index)((strnig), (substring))

CFLAT_DEF isize cflat_sv_find_last_index_cstr(CflatStringView strnig, const char *substring        );
CFLAT_DEF isize cflat_sv_find_last_index_str (CflatStringView strnig, CflatString *substring       );
CFLAT_DEF isize cflat_sv_find_last_index_sv  (CflatStringView strnig, CflatStringView substring    );
CFLAT_DEF isize cflat_sv_find_last_index_sb  (CflatStringView strnig, CflatStringBuilder *substring);

#define cflat_sv_find_last_index(strnig, substring) CFLAT__STRING_OVERLOAD((substring), cflat_sv_find_last_index)((strnig), (substring))

CFLAT_DEF CflatStringView cflat_path_name_cstr(const char         *path);
CFLAT_DEF CflatStringView cflat_path_name_str (CflatString        *path);
CFLAT_DEF CflatStringView cflat_path_name_sb  (CflatStringBuilder *path);
CFLAT_DEF CflatStringView cflat_path_name_sv  (CflatStringView     path);

#define cflat_path_name(path) CFLAT__STRING_OVERLOAD((path), cflat_path_name)((path))

#if defined(CFLAT_IMPLEMENTATION)

CflatStringView cflat_sv_from_cstr(const char       *cstr) { return (CflatStringView) { .data = (char*)cstr, .length = strlen(cstr) }; }
CflatStringView cflat_sv_from_str (CflatString       *str) { return (CflatStringView) { .data = str->data, .length = str->length };    }
CflatStringView cflat_sv_from_sb (CflatStringBuilder *sb)  { return (CflatStringView) { .data = sb->data, .length = sb->count };       }
CflatStringView cflat_sv_from_sv (CflatStringView     sv)  { return sv;                                                                }

CflatString *cflat_str_clone_cstr(CflatArena *a, const char *cstr) {
    const usize cstrlen = strlen(cstr);
    CflatString *result = arena_push(a, sizeof(CflatString) + cstrlen + 1, 0);
    result->length = cstrlen;
    cflat_mem_copy(result->data, cstr, cstrlen);
    result->data[cstrlen] = '\0';
    return result;
}

CflatString *cflat_str_clone_sv(CflatArena *a, CflatStringView sv) {
    CflatString *result = arena_push(a, sizeof(CflatString) + sv.length + 1, 0);
    result->length = sv.length;
    cflat_mem_copy(result->data, sv.data, sv.length);
    result->data[sv.length] = '\0';
    return result;
}

CflatString *cflat_str_clone_str(CflatArena *a, CflatString *str) {
    CflatString *result = arena_push(a, sizeof(CflatString) + str->length + 1, 0);
    result->length = str->length;
    cflat_mem_copy(result->data, str->data, str->length);
    result->data[str->length] = '\0';
    return result;
}

CflatString *cflat_str_clone_sb(CflatArena *a, CflatStringBuilder *sb) {
    CflatString *result = arena_push(a, sizeof(CflatString) + sb->count + 1, 0);
    result->length = sb->count;
    cflat_mem_copy(result->data, sb->data, sb->count);
    result->data[sb->count] = '\0';
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

isize cflat_sv_find_index_cstr(CflatStringView string, const char *substring)       { return cflat_sv_find_index_sv(string, cflat_sv_from_cstr(substring)); }
isize cflat_sv_find_index_str(CflatStringView string, CflatString *substring)       { return cflat_sv_find_index_sv(string, cflat_sv_from_str(substring));  }
isize cflat_sv_find_index_sb(CflatStringView string, CflatStringBuilder *substring) { return cflat_sv_find_index_sv(string, cflat_sv_from_sb(substring));   }

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

isize cflat_sv_find_last_index_cstr(CflatStringView string, const char *substring)         { return cflat_sv_find_last_index_sv(string, cflat_sv_from_cstr(substring)); }
isize cflat_sv_find_last_index_str (CflatStringView string, CflatString *substring)        { return cflat_sv_find_last_index_sv(string, cflat_sv_from_str(substring));  }
isize cflat_sv_find_last_index_sb  (CflatStringView string, CflatStringBuilder *substring) { return cflat_sv_find_last_index_sv(string, cflat_sv_from_sb(substring));   }

CflatStringView cflat_sv_find_substring_sv(CflatStringView string, CflatStringView substring) {
    isize index = cflat_sv_find_index_sv(cflat_sv_from_sv(string), cflat_sv_from_sv(substring));
    if (index < 0) return (CflatStringView){0};
    return (CflatStringView) {
        .data = string.data + index,
        .length = string.length - index
    };
}

CflatStringView cflat_sv_find_substring_cstr(CflatStringView string, const char *substring) {
    isize index = cflat_sv_find_index_sv(cflat_sv_from_sv(string), cflat_sv_from_cstr(substring));
    if (index < 0) return (CflatStringView){0};
    return (CflatStringView) {
        .data = string.data + index,
        .length = string.length - index
    };
}

CflatStringView cflat_sv_find_substring_str(CflatStringView string, CflatString *substring) {
    isize index = cflat_sv_find_index_sv(cflat_sv_from_sv(string), cflat_sv_from_str(substring));
    if (index < 0) return (CflatStringView){0};
    return (CflatStringView) {
        .data = string.data + index,
        .length = string.length - index
    };
}

CflatStringView cflat_sv_find_substring_sb(CflatStringView string, CflatStringBuilder *substring) {
    isize index = cflat_sv_find_index_sv(cflat_sv_from_sv(string), cflat_sv_from_sb(substring));
    if (index < 0) return (CflatStringView){0};
    return (CflatStringView) {
        .data = string.data + index,
        .length = string.length - index
    };
}

CflatStringView cflat_path_name_sv(CflatStringView path) {
#if defined(OS_WINDOWS)
    CflatStringView p1 = cflat_sv_find_substring_sv(path, (CflatStringView) { .data = "/" ,  .length = 1 });
    CflatStringView p2 = cflat_sv_find_substring_sv(path, (CflatStringView) { .data = "\\", .length  = 1 });
    CflatStringView p = (p1.data > p2.data) ? p1 : p2;
    return (CflatStringView) {
        .data   = p.data ? p.data   + 1 : path.data,
        .length = p.data ? p.length - 1 : path.length
    };
#else
    CflatStringView p = cflat_find_substring_sv(path, (CflatStringView) { .data = "/" ,  .length = 1 });
    return (CflatStringView) {
        .data   = p.data ? p.data   + 1 : path.data,
        .length = p.data ? p.length - 1 : path.length
    };
#endif // _WIN32
}

CflatStringView cflat_path_name_str  (CflatString *path)        { return cflat_path_name_sv(cflat_sv_from_str(path));  }
CflatStringView cflat_path_name_cstr (const char *path)         { return cflat_path_name_sv(cflat_sv_from_cstr(path)); }
CflatStringView cflat_path_name_sb   (CflatStringBuilder *path) { return cflat_path_name_sv(cflat_sv_from_sb(path));   }

#endif // CFLAT_IMPLEMENTATION


#if !defined(CFLAT_STRING_NO_ALIAS)
#   define sv_lit cflat_sv_lit
#   define sv_from cflat_sv_from
#   define str_clone cflat_str_clone
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
