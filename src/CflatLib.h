#ifndef CFLAT_OS_H
#define CFLAT_OS_H

#include "CflatArena.h"
#include "CflatCore.h"
#include "CflatString.h"
#include "minwindef.h"
#include "winnt.h"
#include <stdio.h>

typedef u64 CflatOsHandle;
typedef void(CflatProcedure)(void);

typedef struct clfat_lib {
    CflatOsHandle handle;
    #if defined(OS_WINDOWS)
    CflatOsHandle  h_dll;
    CflatOsHandle  h_pdb;
    char temp_dll[MAX_PATH];
    char temp_pdb[MAX_PATH];
    #endif
} CflatLib;

#define CflatOsHandleIsValid(OS_HANDLE) ((OS_HANDLE) != 0)

CFLAT_DEF CflatLib cflat_lib_open(const char *path);
CFLAT_DEF bool cflat_lib_close(CflatLib lib);
CFLAT_DEF CflatProcedure* cflat_load_symbol(CflatLib lib, const char *name);

#if defined(CFLAT_IMPLEMENTATION)

#if defined(OS_WINDOWS)

typedef void *HWND;
#include <libloaderapi.h>
#include <fileapi.h>
#include <errhandlingapi.h>
#include <winbase.h>

#if !defined(CopyFile)
#   if defined(UNICODE)
    WINBASEAPI BOOL WINAPI CopyFileW(_In_ LPCWSTR lpExistingFileName, _In_ LPCWSTR lpNewFileName, _In_ BOOL bFailIfExists);
#   define CopyFile  CopyFileW
#   else
    WINBASEAPI BOOL WINAPI CopyFileA(_In_ LPCSTR lpExistingFileName, _In_ LPCSTR lpNewFileName, _In_ BOOL bFailIfExists);
#   define CopyFile  CopyFileA
#   endif
#endif

void CflatPrintWin32Error() {
    DWORD errorID = GetLastError();
    if (errorID == 0) {
        return; // No error occurred
    }

    LPSTR messageBuffer = NULL;

    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, 
        errorID, 
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
        (LPSTR)&messageBuffer, 
        0, 
        NULL);

    if (size > 0) {
        printf("Win32 Error %lu: %s", errorID, messageBuffer);
    } else {
        printf("Win32 Error %lu (Failed to retrieve description)\n", errorID);
    }

    LocalFree(messageBuffer);
}

CflatLib cflat_lib_open(const char *path) {
    if (path == NULL) {
        return (CflatLib){ 
            .handle = (CflatOsHandle)GetModuleHandleA(NULL), 
            .h_dll  = (CflatOsHandle)INVALID_HANDLE_VALUE, 
            .h_pdb  = (CflatOsHandle)INVALID_HANDLE_VALUE 
        };
    }

    CflatLib lib = {0};
    char orig_pdb[MAX_PATH];
    char base_name[MAX_PATH];

    strncpy_s(base_name, MAX_PATH, path, _TRUNCATE);
    char* dot = strrchr(base_name, '.');
    
    strncpy_s(orig_pdb, MAX_PATH, path, _TRUNCATE);
    char* p_ext = strrchr(orig_pdb, '.');
    if (p_ext) strcpy_s(p_ext, 5, ".pdb");

    DWORD pid = GetCurrentProcessId();
    if (dot) {
        *dot = '\0';
        sprintf_s(lib.temp_dll, MAX_PATH, "%s%u.dll", base_name, pid);
        sprintf_s(lib.temp_pdb, MAX_PATH, "%s%u.pdb", base_name, pid);
    } else {
        sprintf_s(lib.temp_dll, MAX_PATH, "%s%u.dll", path, pid);
        sprintf_s(lib.temp_pdb, MAX_PATH, "%s%u.pdb", path, pid);
    }

    if (!CopyFileA(path, lib.temp_dll, FALSE)) return (CflatLib){0};
    CopyFileA(orig_pdb, lib.temp_pdb, FALSE); 

    lib.handle = (CflatOsHandle)LoadLibraryA(lib.temp_dll);
    if (!lib.handle) {
        DeleteFileA(lib.temp_dll);
        DeleteFileA(lib.temp_pdb);
        return (CflatLib){0};
    }

    lib.h_dll = (CflatOsHandle)CreateFileA(lib.temp_dll, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
                             NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    lib.h_pdb = (CflatOsHandle)CreateFileA(lib.temp_pdb, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
                             NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);

    return lib;
}

bool cflat_lib_close(CflatLib lib) {
    if (!lib.handle || lib.handle == (CflatOsHandle)GetModuleHandleA(NULL)) return false;

    BOOL freed = FreeLibrary((HMODULE)lib.handle);
    
    if ((HANDLE)lib.h_dll != INVALID_HANDLE_VALUE) CloseHandle((HANDLE)lib.h_dll);
    if ((HANDLE)lib.h_pdb != INVALID_HANDLE_VALUE) CloseHandle((HANDLE)lib.h_pdb);
    
    DeleteFileA(lib.temp_dll);
    DeleteFileA(lib.temp_pdb);
    
    return freed != 0;
}

CflatProcedure* cflat_load_symbol(CflatLib lib, const char *name) {
    return (CflatProcedure*)GetProcAddress((HMODULE)lib.handle, name);
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