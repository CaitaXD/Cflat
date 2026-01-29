#include <stdio.h>
#include <string.h>
#define DEBUG 1

#if !defined(CFLAT_IMPLEMENTATION)
#   define CFLAT_IMPLEMENTATION
#endif

#define CFLAT_DEF static inline
#include "../src/CflatLib.h"
#include "unitest.h"

#define EXPORT __declspec(dllexport)

EXPORT char *hello(const char *name) {
    char *buffer = calloc(1, strlen(name) + strlen("Hello ") + 1);
    memcpy(buffer, "Hello ", strlen("Hello "));
    memcpy(buffer + strlen("Hello "), name, strlen(name));
    buffer[strlen(name) + strlen("Hello ")] = '\0';
    return buffer;
}

void os_library_open_should_work(void) {
    // Arrange
    // Act
    OsHandle handle = lib_open(NULL);
    // Assert
    ASSERT_NOT_NULL(handle);
}

void os_library_load_symbol_should_work(void) {
    // Arrange
    OsHandle handle = lib_open(NULL);
    // Act
    Procedure *ptr = load_symbol(handle, "hello");
    char *result = ((char* (*) (const char*))ptr) ("from dynamically loaded symbol");
    // Assert
    ASSERT_NOT_NULL((uptr)ptr);
    ASSERT_TRUE(strncmp(result, "Hello from dynamically loaded symbol", strlen("Hello from dynamically loaded symbol")) == 0);
}

int main(void) {
    typedef void testfn(void);
    testfn *tests[] = {
        os_library_open_should_work,
        os_library_load_symbol_should_work,
    };

    const usize test_count = CFLAT_ARRAY_SIZE(tests);

    for (usize i = 0; i < test_count; ++i) {
        TempArena tmp;
        scratch_arena_scope(tmp) {
            tests[i]();
        }
    }

    printf("All tests passed ✅\n");

    return 0;
}
