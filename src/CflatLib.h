#ifndef CFLAT_OS_H
#define CFLAT_OS_H

#include "CflatArena.h"
#include "CflatCore.h"
#include "CflatString.h"
#include <stdio.h>

typedef u64 CflatOsHandle;
typedef void(CflatProcedure)(void);

#define CflatOsHandleIsValid(OS_HANDLE) ((OS_HANDLE) != 0)

CFLAT_DEF CflatOsHandle cflat_lib_open_cstr(const char *path);
CFLAT_DEF CflatOsHandle cflat_lib_open_str (CflatString *path);

#define cflat_lib_open(path) _Generic((path)    \
    , void*:            cflat_lib_open_cstr     \
    , char*:            cflat_lib_open_cstr     \
    , const char*:      cflat_lib_open_cstr     \
)((path))

CFLAT_DEF CflatProcedure* cflat_load_symbol_cstr(CflatOsHandle lib, const char *name);
CFLAT_DEF CflatProcedure* cflat_load_symbol_str (CflatOsHandle lib, CflatString *name);

#define cflat_load_symbol(lib, name) _Generic((name)    \
    , void*:            cflat_load_symbol_cstr     \
    , char*:            cflat_load_symbol_cstr     \
    , const char*:      cflat_load_symbol_cstr     \
)((lib), (name))

CFLAT_DEF bool cflat_lib_close(CflatOsHandle lib);

#if defined (CFLAT_IMPLEMENTATION)

#if defined(OS_WINDOWS)

#include <libloaderapi.h>
#include <fileapi.h>


#if !defined(CopyFile)
#   if defined(UNICODE)
    WINBASEAPI BOOL WINAPI CopyFileW(_In_ LPCWSTR lpExistingFileName, _In_ LPCWSTR lpNewFileName, _In_ BOOL bFailIfExists);
#   define CopyFile  CopyFileW
#   else
    WINBASEAPI BOOL WINAPI CopyFileA(_In_ LPCSTR lpExistingFileName, _In_ LPCSTR lpNewFileName, _In_ BOOL bFailIfExists);
#   define CopyFile  CopyFileA
#   endif
#endif


CflatOsHandle cflat_lib_open_cstr(const char *path) {
    if (path == NULL) {
        return (u64)GetModuleHandleA(NULL);
    }

    char temp_path_buffer     [MAX_PATH];
    char temp_file_name_buffer[MAX_PATH];
    
    GetTempPath(MAX_PATH, temp_path_buffer);
    GetTempFileName(temp_path_buffer, "cflat", 1, temp_file_name_buffer); 
    CopyFile(path, temp_file_name_buffer, false);
    
    CflatOsHandle lib = (u64)LoadLibrary(temp_file_name_buffer);
    
    DeleteFile(temp_file_name_buffer);
    return lib;
}

CflatOsHandle cflat_lib_open_str(CflatString *path) {
    return cflat_lib_open_cstr(path->data);
}

CflatProcedure* cflat_load_symbol_cstr(CflatOsHandle lib, const char *name) {
    return (CflatProcedure*)GetProcAddress((HMODULE)lib, name);
}

CflatProcedure* cflat_load_symbol_str(CflatOsHandle lib, CflatString *name) {
    return (CflatProcedure*)GetProcAddress((HMODULE)lib, name->data);
}


bool cflat_lib_close(CflatOsHandle lib)
{
    return FreeLibrary((HMODULE)lib);
}

#elif defined(OS_UNIX)
#endif


#endif // CFLAT_IMPLEMENTATION

#ifndef CFLAT_OS_NO_ALIAS
#   define lib_open cflat_lib_open
#   define load_symbol cflat_load_symbol
#   define lib_close cflat_lib_close
#   define OsHandle CflatOsHandle
#   define Procedure CflatProcedure
#   define OsHandleIsValid CflatOsHandleIsValid
#endif // CFLAT_OS_NO_ALIAS

#endif //CFLAT_OS_H