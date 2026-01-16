#ifndef CFLAT_OS_H
#define CFLAT_OS_H

#include "CflatArena.h"
#include "CflatCore.h"
#include "CflatString.h"
#include <minwindef.h>

typedef u64 CflatOsHandle;

CFLAT_DEF CflatOsHandle (cflat_lib_open)(CflatStringView path);
#define cflat_lib_open(path) (cflat_lib_open)(sv_lit(path))

CFLAT_DEF void* (cflat_load_symbol)(CflatOsHandle lib, CflatStringView name);
#define cflat_load_symbol(lib, name) (cflat_load_symbol)((lib), sv_lit(name))

CFLAT_DEF void cflat_lib_close(CflatOsHandle lib);

#if defined (CFLAT_IMPLEMENTATION)

#if defined(OS_WINDOWS)

#include <libloaderapi.h>

CflatOsHandle (cflat_lib_open)(const CflatStringView path) {
    
    if (path.data == NULL) return (CflatOsHandle)GetModuleHandleA(NULL);

    char path_buffer[MAX_PATH];
    cflat_mem_copy(path_buffer, path.data, path.length);
    path_buffer[path.length] = '\0';

    return (CflatOsHandle)LoadLibraryA(path_buffer);
}

void* (cflat_load_symbol)(CflatOsHandle lib, CflatStringView name)
{
    CflatTempArena scratch;
    char *name_buffer;
    void *result;
    scratch_arena_scope(scratch) {
        name_buffer = arena_push(scratch.arena, name.length + 1);
        cflat_mem_copy(name_buffer, name.data, name.length);
        name_buffer[name.length] = '\0';
        result = GetProcAddress((HMODULE)lib, name_buffer);
    }
    return result;
}

void cflat_lib_close(CflatOsHandle lib)
{
  FreeLibrary((HMODULE)lib);
}


#elif defined(OS_UNIX)
#endif


#endif // CFLAT_IMPLEMENTATION

#ifndef CFLAT_OS_NO_ALIAS
#   define lib_open cflat_lib_open
#   define load_symbol cflat_load_symbol
#   define lib_close cflat_lib_close
#   define OsHandle CflatOsHandle
#endif // CFLAT_OS_NO_ALIAS

#endif //CFLAT_OS_H