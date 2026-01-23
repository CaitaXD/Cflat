#include <minwindef.h>
#include <stdio.h>
#define DEBUG 1

#if !defined(CFLAT_IMPLEMENTATION)
#   define CFLAT_IMPLEMENTATION
#endif

#define CFLAT_DEF static inline
#include "../src/Cflat.h"
#include "unitest.h"

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
    for (const CflatArenaNode *curr = a->free; curr; curr = curr->prev) {
        freelist_count += 1;
    }
    // Act
    arena_push(a, a->curr->res, 1, true);
    arena_pop(a, a->curr->res);
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
    typedef define_da(i32, nda) NumbersDa;
    NumbersDa *xs = da_new(NumbersDa, a, 0);
    // Act
    for (i32 i = 0; i < 100000; ++i) {
        arena_da_append(a, xs, i);
        // Assert
        ASSERT_EQUAL(xs->data[i], i, "%d");
    }
}

void da_clone_should_work(void) {
    // Arrange
    typedef define_da(i32, nda) NumbersDa;
    NumbersDa *xs = da_lit(NumbersDa, 100000);
    const isize len = 100000;
    for (i32 i = 0; i < len; ++i) {
        arena_da_append(a, xs, i);
    }
    // Act
    const NumbersDa *ys = da_clone(a, xs, 0);
    // Assert
    ASSERT_EQUAL(xs->count, ys->count, "%zu");
    for (i32 i = 0; i < len; ++i) {
        ASSERT_EQUAL(ys->data[i], i, "%d");
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

void da_alloc_jagged_array_leak_test(void) {
    // Arrange
    typedef define_da(i32, nda) NumbersDa;
    typedef define_da(NumbersDa*, pnda) JaggedDa;
    JaggedDa *xs = da_new(JaggedDa, a, 0);
    // Act
    arena_da_append(a, xs, da_new(NumbersDa, a, 0));
    arena_da_append(a, xs, da_new(NumbersDa, a, 0));
    arena_da_append(a, da_get(xs, 0), 2);
    arena_da_append(a, da_get(xs, 0), 4);
    arena_da_append(a, da_get(xs, 1), 3);
    // Assert
    ASSERT_EQUAL(da_get(da_get(xs, 0), 0), 2, "%d");
    ASSERT_EQUAL(da_get(da_get(xs, 0), 1), 4, "%d");
    ASSERT_EQUAL(da_get(da_get(xs, 1), 0), 3, "%d");
}

void array_init_should_work(void) {
    // Arrange
    typedef define_array(i32, nda) NumbersArray;
    const usize len = 100000;
    void *stackp = stackalloc(array_allocation_size(i32, 100000));
    NumbersArray *xs = array_init(NumbersArray, stackp, len, 0);
    // Act
    for (u32 i = 0; i < xs->length; ++i) {
        xs->data[i] = (i32) i;
    }
    // Assert
    ASSERT_NOT_NULL(xs);
    ASSERT_EQUAL(xs->length, (usize)len, "%zu");
    for (u32 i = 0; i < xs->length; ++i) {
        ASSERT_EQUAL(xs->data[i], (i32)i, "%d");
    }
}

void array_new_should_work(void) {
    // Arrange
    typedef define_array(i32, nda) NumbersArray;
    const usize len = 100000;
    NumbersArray *xs = array_new(NumbersArray, len, a, 0);
    assert(xs);
    // Act
    for (u32 i = 0; i < xs->length; ++i) {
        xs->data[i] = (i32) i;
    }
    // Assert
    ASSERT_NOT_NULL(xs);
    ASSERT_EQUAL(xs->length, (usize)len, "%zu");
    for (u32 i = 0; i < xs->length; ++i) {
        ASSERT_EQUAL(xs->data[i], (i32)i, "%d");
    }
}

u64 mock_hash(u64 val) {
    return val * 11400714819323198485ULL;
}

void test_hashset_basic_operations(void) {
    
    typedef cflat_define_hashset(int, int_hashset) IntHashSet;
    IntHashSet *hs = cflat_hashset_new(IntHashSet, a, .capacity = 4);
    
    ASSERT_NOT_NULL(hs);
    ASSERT_EQUAL(hs->count, (usize)0, "%zu");

    // Test Adding Elements
    int val1 = 42;
    u64 hash1 = mock_hash(val1);
    bool added = cflat_hashset_add(a, hs, val1, hash1);
    
    ASSERT_TRUE(added);
    ASSERT_EQUAL(hs->count, (usize)1, "%zu");

    // Test Duplicates (should return false)
    bool added_again = cflat_hashset_add(a, hs, val1, hash1);
    ASSERT_FALSE(added_again);
    ASSERT_EQUAL(hs->count, (usize)1, "%zu");

    // Test Lookup
    u64 index;
    bool found = cflat_hashset_index(hs, val1, hash1, &index);
    ASSERT_TRUE(found);
    
    int val2 = 100;
    bool found_missing = cflat_hashset_index(hs, val2, mock_hash(val2), &index);
    ASSERT_FALSE(found_missing);
}

void test_hashset_resizing(void) {
    const usize initial_capacity = 2;
    const usize N  = 10;
    typedef cflat_define_hashset(u64, u64_hashset) U64HashSet;
    U64HashSet *hs = cflat_hashset_new(U64HashSet, a, .capacity = initial_capacity);
    
    usize original_exponent = hs->exponent;

    // Add elements until it resizes (Load factor 0.75)
    for (u64 i = 0; i < N; ++i) {
        cflat_hashset_add(a, hs, i, mock_hash(i));
    }

    ASSERT_GREATER_THAN(hs->exponent, original_exponent, "%zu");
    ASSERT_EQUAL(hs->count, (usize)N, "%zu");

    for (u64 i = 0; i < N; ++i) {
        u64 idx;
        bool found = cflat_hashset_index(hs, i, mock_hash(i), &idx);
        ASSERT_TRUE(found);
    }
}

void test_hashset_tombstone_integrity(void) {
    typedef cflat_define_hashset(int, int_hashset) IntHashSet;
    IntHashSet *hs = cflat_hashset_new(IntHashSet, a, .capacity = 8);

    // Assuming 'a' and 'b' will collide
    int na = 10, nb = 18;
    u64 hash_a = 2, hash_b = 2; 

    cflat_hashset_add(a, hs, na, hash_a);
    cflat_hashset_add(a, hs, nb, hash_b);

    // Delete 'a' and verify 'b' is still findable
    u64 idx;
    cflat_hashset_index(hs, na, hash_a, &idx);
    CflatHashSetOpaqueEntry *entry_a = cflat__hashset_get_entry(sizeof(int), (CflatOpaqueHashSet*)hs, idx);
    entry_a->flags = CFLAT__HSE_DELETED;
    hs->count--;

    bool found_b = cflat_hashset_index(hs, nb, hash_b, &idx);
    ASSERT_TRUE(found_b);
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
        da_clone_should_work,
        arena_delete_wont_free_stack_memory,
        da_alloc_jagged_array_leak_test,
        array_init_should_work,
        array_new_should_work,
        test_hashset_basic_operations,
        test_hashset_resizing,
        test_hashset_tombstone_integrity,
    };

    const usize test_count = CFLAT_ARRAY_SIZE(tests);

    for (usize i = 0; i < test_count; ++i) {
        a = arena_new();
        tests[i]();
        arena_delete(a);
    }

    for (usize i = 0; i < test_count; ++i) {
        TempArena tmp;
        scratch_arena_scope(tmp) {
            a = tmp.arena;
            tests[i]();
        }
    }

    arena_delete(cflat__tls_scratches[0]);
    arena_delete(cflat__tls_scratches[1]);

    printf("All tests passed ✅\n");

    return 0;
}
