#ifndef CFLAT_HASH_SET_H
#define CFLAT_HASH_SET_H

#include "CflatArena.h"
#include "CflatCore.h"
#include <stdint.h>
#include <string.h>

cflat_enum(HashSetEntryFlag, u64) {
    CFLAT__HSE_EMPTY      = 0,
    CFLAT__HSE_USED       = 1,
    CFLAT__HSE_DELETED    = 1 << 1,
};

typedef struct cflat_hash_set_opaque_entry {
    HashSetEntryFlag flags;
    u64 hash;
    byte value[];
} CflatHashSetOpaqueEntry;

#define cflat_hashset_fields(T)                                                                                                               \
    usize exponent;                                                                                                                           \
    usize count;                                                                                                                              \
    cflat_alignas(uptr) struct {                                                                                                              \
        HashSetEntryFlag flags;                                                                                                               \
        u64 hash;                                                                                                                             \
        T value;                                                                                                                              \
    } data[]

#define cflat_define_hashset(T, ...) struct __VA_ARGS__ {                                                                                     \
    cflat_hashset_fields(T);                                                                                                                  \
}

typedef cflat_define_hashset(byte, cflat_opaque_hashset) CflatOpaqueHashSet;

#define cflat__hashset_element_size(HS)                 (sizeof((HS)->data->value))
#define cflat__hashset_check_ptr(HS)                    (cflat_static_assert(sizeof(CflatOpaqueHashSet) == sizeof(*(HS))), (void*)(HS))
#define cflat__hashset_check_dptr(HS)                   (cflat_static_assert(sizeof(CflatOpaqueHashSet) == sizeof(**(HS))), (void*)(HS))
#define cflat__hashset_check_value(HS, VALUE)           (cflat_static_assert(sizeof((VALUE)) == cflat__hashset_element_size((HS))), (void*)cflat_plit(VALUE))

typedef struct cflat_hashset_new_opt {
    usize capacity;
} CflathashsetNewOpt;

CFLAT_DEF CflatOpaqueHashSet* cflat_hashset_new_opt(usize element_size, CflatArena *a, CflathashsetNewOpt opt);

#define cflat_hashset_new(THashSet, arena, ...)\
    (THashSet*)cflat_hashset_new_opt(cflat_sizeof_member(THashSet, data->value), (arena), OVERRIDE_INIT(CflathashsetNewOpt, .capacity = 256, __VA_ARGS__))

CFLAT_DEF bool (cflat_hashset_index)(usize element_size, CflatOpaqueHashSet *hashset, byte element[element_size], u64 hash, u64 *out_index);

#define cflat_hashset_index(HS, VALUE, HASH, OUT_INDEX)                                 \
    (cflat_hashset_index)(                                                              \
        cflat__hashset_element_size((HS)),                                              \
        cflat__hashset_check_ptr((HS)),                                                 \
        cflat__hashset_check_value((HS), (VALUE)),                                      \
        (HASH),                                                                         \
        (OUT_INDEX)                                                                     \
    )
    
CFLAT_DEF bool (cflat_hashset_add)(usize element_size, CflatArena *a, CflatOpaqueHashSet **hashset, byte element[element_size], u64 hash);

#define cflat_hashset_add(A, HS, VALUE, HASH)                                           \
    (cflat_hashset_add)(                                                                \
        cflat__hashset_element_size((HS)),                                              \
        (A),                                                                            \
        cflat__hashset_check_dptr(&(HS)),                                               \
        cflat__hashset_check_value((HS), (VALUE)),                                      \
        (HASH)                                                                          \
    )

#if defined(CFLAT_IMPLEMENTATION)

CflatHashSetOpaqueEntry* cflat__hashset_get_entry(usize element_size, CflatOpaqueHashSet *hashset, usize index) {
    const usize word_aling = cflat_alignof(uptr);
    const uptr entry = (uptr)hashset->data;
    const usize entry_size = cflat_align_pow2(sizeof(CflatHashSetOpaqueEntry) + element_size, word_aling);
    CflatHashSetOpaqueEntry *ret = (void*)(entry + entry_size*index);
    return ret;
}

CFLAT_DEF CflatOpaqueHashSet* cflat_hashset_new_opt(usize element_size, CflatArena *a, CflathashsetNewOpt opt) {
    const usize word_aling = cflat_alignof(uptr);
    const usize capacity = next_pow2(opt.capacity);
    const usize entry_size = cflat_align_pow2(sizeof(CflatHashSetOpaqueEntry) + element_size, word_aling);
    const usize bytes = sizeof(CflatOpaqueHashSet) + entry_size*capacity;
    CflatOpaqueHashSet *hashset = cflat_arena_push_opt(a, bytes, (CflatAllocOpt) { .clear = true });
    hashset->exponent = cflat_log2_u64(capacity);
    hashset->count = 0;
    return hashset;
}

#define cflat_hashset_ensure_capacity(arena, hashset, capacity)                                                                                             \
    (cflat_typeof(hashset))                                                                                                                                 \
    (cflat_hashset_ensure_capacity)(sizeof(*(hashset)->data), (arena), (void*)(hashset), (capacity))

u64 cfalt__double_hashing(u64 hash, u64 exponential, i32 last_index)
{
    cflat_assert(exponential < 64);
    u64 mask = (1ULL << exponential) - 1;
    u64 step = (hash >> (64U - exponential)) | 1;
    return (last_index + step) & mask;
}

bool (cflat_hashset_index)(usize element_size, CflatOpaqueHashSet *hashset, byte element[element_size], u64 hash, u64 *out_index) {
    cflat_assert(element != NULL);
    if (hashset == NULL) {
        if(out_index) *out_index = SIZE_MAX;
        return false;
    }

    if(out_index) *out_index = 0;
    for (u64 i = hash;;) {
        i = cfalt__double_hashing(hash, hashset->exponent, i);
        CflatHashSetOpaqueEntry *entry = cflat__hashset_get_entry(element_size, hashset, i);

        if (entry->flags == CFLAT__HSE_EMPTY) {
            if(out_index) *out_index = *out_index ? *out_index : i;
            return false;
        }
        else if (entry->flags == CFLAT__HSE_DELETED) {
            if(out_index) *out_index = *out_index ? *out_index : i;
        }
        else if (memcmp(entry->value, element, element_size) == 0) {
            *out_index = i;
            return true;
        }
    }
}

void cflat__hashset_add_internal(usize element_size, CflatOpaqueHashSet *hashset, byte value[element_size], u64 hash) {
    cflat_assert(hashset != NULL && value != NULL);
    cflat_assert(hashset->count < (1ULL << hashset->exponent));
    u64 index;
    bool exists = (cflat_hashset_index)(element_size, hashset, value, hash, &index);

    if (!exists) {
        cflat_assert(index != SIZE_MAX);
        CflatHashSetOpaqueEntry *entry = cflat__hashset_get_entry(element_size, hashset, index);        
        entry->flags = CFLAT__HSE_USED;
        entry->hash = hash;
        hashset->count++;
        cflat_mem_move(entry->value, value, element_size);
    }
}

CFLAT_DEF CflatOpaqueHashSet* (cflat_hashset_ensure_capacity)(usize element_size, CflatArena *a, CflatOpaqueHashSet *hashset, usize capacity) {
    const usize old_capacity = (1ULL << hashset->exponent);
    if (old_capacity >= capacity) return hashset;
    
    const usize word_aling = cflat_alignof(uptr);
    const usize entry_size = cflat_align_pow2(sizeof(CflatHashSetOpaqueEntry) + element_size, word_aling);
    const usize new_capacity = next_pow2(capacity);
    const usize old_size = sizeof(*hashset) + entry_size*old_capacity;
    const usize new_size = sizeof(*hashset) + entry_size*new_capacity;

    CflatOpaqueHashSet *new_hashset = cflat_arena_extend_opt(a, hashset, old_size, new_size, (CflatAllocOpt) { .align = cflat_alignof(uptr), .clear = true});

    CflatTempArena temp;
    arena_scratch_scope(temp) {
        if (new_hashset == hashset) {
            CflatOpaqueHashSet *old_hashset = arena_push(temp.arena, sizeof(*new_hashset) + entry_size*old_capacity, .align = word_aling, .clear = false);
            cflat_mem_copy(old_hashset, new_hashset, sizeof(*new_hashset) + entry_size*old_capacity);
            hashset = old_hashset;
        }

        cflat_mem_zero(new_hashset, sizeof(*new_hashset) + entry_size*new_capacity);
        new_hashset->exponent = cflat_log2_u64(new_capacity);

        for (u64 i = 0; i < old_capacity; ++i) {
            CflatHashSetOpaqueEntry *old_entry = cflat__hashset_get_entry(element_size, hashset, i);
            if (old_entry->flags == CFLAT__HSE_USED) {
                cflat__hashset_add_internal(element_size, new_hashset, old_entry->value, old_entry->hash);
            }
        }
    }

    return new_hashset;
}

// Returns true if the value was added, false if it was already present
// If Arena is NULL, returns what would be the result of adding the value without actually adding it to the hashset
bool (cflat_hashset_add)(usize element_size, CflatArena *a, CflatOpaqueHashSet **hashset, byte value[element_size], u64 hash) {
    cflat_assert(hashset != NULL && value != NULL);
    u64 index;
    bool exists;
    if (a == NULL) {
        exists = (cflat_hashset_index)(element_size, *hashset, value, hash, &index);
        return !exists;
    }

    // Cheking the capacity before checking the if the element exists here because if the set gets resized, the index will be invalidated
    const usize load_factor = (usize)(4*(*hashset)->count/3) + 1;
    *hashset = (cflat_hashset_ensure_capacity)(element_size, a, *hashset, load_factor);
    exists = (cflat_hashset_index)(element_size, *hashset, value, hash, &index);

    if (!exists) {
        cflat_assert(index != SIZE_MAX);
        CflatHashSetOpaqueEntry *entry = cflat__hashset_get_entry(element_size, *hashset, index);        
        entry->flags = CFLAT__HSE_USED;
        entry->hash = hash;
        (*hashset)->count++;
        cflat_mem_move(entry->value, value, element_size);
        return true;
    }
    return false;
}

#endif // CFLAT_IMPLEMENTATION

#endif //CFLAT_HASH_SET_H