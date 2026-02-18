#include <minwindef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winnt.h>
#define DEBUG 1

#if !defined(CFLAT_IMPLEMENTATION)
#   define CFLAT_IMPLEMENTATION
#endif

#define CFLAT_DEF static inline
#include "../src/Cflat.h"
#include "unitest.h"

typedef struct {
    CFLAT_SLICE_FIELDS(i32);
} i32Slice;

Arena *a;

void arena_push_should_create_new_block(void) {
    // Arrange
    const CflatArenaNode *prev = a->curr;
    const usize cap = a->curr->res + 1;
    // Act
    arena_push(a, cap, 1, true);
    // Assert
    ASSERT_NOT_EQUAL((void*)prev, (void*)a->curr, "%p");
    ASSERT_NOT_NULL((void*)
        ((uptr)a->curr | (uptr)a->free)
    );
}

void arena_clear_should_clear(void) {
    // Arrange
    // Act
    arena_push(a, sizeof(i32), 1, true);
    arena_clear(a);
    // Assert
    ASSERT_NOT_NULL(a->curr);
    ASSERT_NULL(a->curr->prev);
}

void free_list_has_sufficiently_large_block(void) {
    // Arrange
    const usize cap = a->curr->res + 1;
    // Act
    arena_push(a, cap, 1, true);
    arena_clear(a);
    arena_push(a, cap, 1, true);
    // Assert
    ASSERT_GREATER_OR_EQUAL(a->curr->res, cap, "%zu");
}

void alloc_should_reset_len_when_taking_from_free_list(void) {
    // Arrange
    // Act
    a->curr->pos += 1;
    arena_clear(a);
    // Assert
    ASSERT_EQUAL(a->curr->pos, sizeof(struct cflat_arena_node), "%zu");
}

void free_list_does_not_have_sufficiently_large_block(void) {
    // Arrange
    const usize cap = a->curr->res + 1;
    const usize double_cap = 4 * a->curr->res;
    // Act
    arena_push(a, cap, 1, true);
    arena_clear(a);
    arena_push(a, double_cap, 1, true);
    // Assert
    ASSERT_GREATER_OR_EQUAL(a->curr->res, double_cap, "%zu");
    ASSERT_LESS_OR_EQUAL(a->free->res, double_cap, "%zu");
}

void dealloc_should_offset_len(void) {
    // Arrange
    const usize base = a->curr->pos;
    // Act
    arena_push(a, sizeof(i32), 1, true);
    arena_pop(a, sizeof(i8));
    // Assert
    ASSERT_EQUAL(a->curr->pos, base + 3 * sizeof(i8), "%zu");
}

void pop_should_free_blocks(void) {
    // Arrange
    usize freelist_count = 0;
    usize max_res = a->curr->res;
    for (const CflatArenaNode *curr = a->free; curr; curr = curr->prev) {
        freelist_count += 1;
        max_res = max(max_res, curr->res);
    }
    // Act
    arena_push(a, max_res, 1, true);
    arena_pop(a, max_res);
    usize new_freelist_count = 0;
    for (const CflatArenaNode *curr = a->free; curr; curr = curr->prev) {
        new_freelist_count += 1;
    }
    // Assert
    ASSERT_NOT_NULL(a->free);
    ASSERT_GREATER_THAN(new_freelist_count, freelist_count, "%zu");
}

void arena_da_append_should_work(void) {
    // Arrange
    i32Slice xs = slice_new(i32Slice, a, 0, .clear = false);
    // Act
    for (i32 i = 0; i < 100000; ++i) {
        slice_append(a, &xs, i);
        // Assert
        ASSERT_EQUAL(*slice_at(xs, i), i, "%d");
    }
}

void arena_delete_wont_free_stack_memory(void) {
    // Arrange
    Arena *stack_arena = stackalloc(10000);
    arena_init(stack_arena, 10000);
    // Act
    arena_delete(stack_arena);
    // Assert
}

void subslice_should_work(void) {
    // Arrange
    i32Slice xs = {0};
    slice_append(a, &xs, 1);
    slice_append(a, &xs, 2);
    slice_append(a, &xs, 3);
    slice_append(a, &xs, 4);
    // Act
    i32Slice sub = subslice(i32Slice, xs, 1, 2);
    // Assert
    ASSERT_EQUAL(slice_length(sub),  2ULL, "%zu");
    ASSERT_EQUAL(slice_data(sub)[0], 2, "%d");
    ASSERT_EQUAL(slice_data(sub)[1], 3, "%d");
}

int main(void) {

    typedef void testfn(void);
    testfn *tests[] = {
        arena_push_should_create_new_block,
        arena_clear_should_clear,
        alloc_should_reset_len_when_taking_from_free_list,
        free_list_has_sufficiently_large_block,
        free_list_does_not_have_sufficiently_large_block,
        dealloc_should_offset_len,
        pop_should_free_blocks,
        arena_da_append_should_work,
        arena_delete_wont_free_stack_memory,
        subslice_should_work,
    };

    const usize test_count = CFLAT_ARRAY_SIZE(tests);

    for (usize i = 0; i < test_count; ++i) {
        a = arena_new();
        tests[i]();
        arena_delete(a);
    }

    for (usize i = 0; i < test_count; ++i) {
        TempArena tmp;
        scratch_arena_scope(tmp, 0) {
            a = tmp.arena;
            tests[i]();
        }
    }

    TempArena tmp;
    scratch_arena_scope(tmp, 0) {
        arena_delete(cflat__tls_scratches[0]);
        arena_delete(cflat__tls_scratches[1]);
    }

    printf("All tests passed ✅\n");

    return 0;
}


#undef HASH_FN
#undef EQUAL_FN
