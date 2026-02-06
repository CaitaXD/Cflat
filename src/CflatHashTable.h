#ifndef CFLAT_HASH_SET_H
#define CFLAT_HASH_SET_H

#include "CflatArena.h"
#include "CflatCore.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define CFLAT_HASHTABLE_FIELDS(TKey, TValue)    \
    usize exponent;                             \
    usize count;                                \
    HashTableEntryFlag *flags;                  \
    TKey   *keys;                               \
    TValue *values;                             \
    CflatHashFunction *hash;                    \
    CflatEqualsFunction *equals                 \

typedef u64 HashTableEntryFlag;
typedef struct cflat_hashtable CflatHashTable;
typedef struct cflat_hashstable_new_opt CflatHashTableNewOpt;

typedef u64 (CflatHashFunction)  (const void *key);
typedef bool(CflatEqualsFunction)(const void *lhs, const void *rhs);

CFLAT_DEF CflatHashTable* (cflat_hashtable_new_opt)(usize key_size, usize value_size, CflatArena *a, CflatHashTableNewOpt opt);
CFLAT_DEF CflatHashTable* (cflat_hashtable_resize )(usize key_size, usize value_size, CflatArena *a, CflatHashTable  *hashtable, usize capacity);
CFLAT_DEF bool            (cflat_hashtable_add    )(usize key_size, usize value_size, CflatArena *a, CflatHashTable **hashtable, const void *key, void *value);
CFLAT_DEF void*           (cflat_hashtable_get    )(usize key_size, usize value_size, CflatHashTable *hashtable, const void *key);

#define cflat_hashtable_capacity(HT)  (1ULL << ((CflatImplHashTable*)(HT))->exponent)
#define cflat_hashtable_count(HT)     ((CflatImplHashTable*)(HT))->count

#define cflat_hashtable_new(THashTable, ARENA, ...)                                                                                                                     \
    (THashTable*)                                                                                                                                                       \
    (cflat_hashtable_new_opt)(cflat_sizeof_member(THashTable, keys[0]),                                                                                                 \
                              cflat_sizeof_member(THashTable, values[0]),                                                                                               \
                              (ARENA),                                                                                                                                  \
                              OVERRIDE_INIT(CflatHashTableNewOpt, .capacity = 256, __VA_ARGS__))

#define cflat_hashtable_add(ARENA, HT, KEY, VALUE)                                                                                                                      \
    (bool)                                                                                                                                                              \
    (cflat_hashtable_add)((sizeof((HT)->keys[0])),                                                                                                                      \
                          (sizeof((HT)->values[0])),                                                                                                                    \
                          (ARENA),                                                                                                                                      \
                          (void*)&(HT),                                                                                                                                 \
                          &cflat_lvalue(cflat_typeof((HT)->keys[0]), (KEY)),                                                                                            \
                          &cflat_lvalue(cflat_typeof((HT)->values[0]), (VALUE)))                                                                                        \
    
#define cflat_hashtable_get(HT, KEY)                                                                                                                                    \
    (cflat_typeof((HT)->values))                                                                                                                                        \
    (cflat_hashtable_get)((sizeof((HT)->keys[0])),                                                                                                                      \
                          (sizeof((HT)->values[0])),                                                                                                                    \
                          (void*)(HT),                                                                                                                                  \
                          &cflat_lvalue(cflat_typeof((HT)->keys[0]), (KEY)))                                                                                            \

#if defined(CFLAT_IMPLEMENTATION)

cflat_enum(HashTableEntryFlag, u64) {
    CFLAT__HASH_ENTRY_EMPTY      = 0,
    CFLAT__HASH_ENTRY_USED       = 1,
    CFLAT__HASH_ENTRY_DELETED    = 2,
};

struct cflat_hashtable {
    CFLAT_HASHTABLE_FIELDS(byte, byte);
};

struct cflat_hashstable_new_opt {
    usize capacity;
    CflatHashFunction *hash;
    CflatEqualsFunction *equals;
};

void* cflat__hashtable_add_raw(usize key_size, usize value_size, CflatHashTable *hashtable, const void *key, void *value, usize *out_index);

u64 cflat__fnv1a_hash(usize key_len, const byte key[key_len]) {
    u64 h = 0x100;
    for (u64 i = 0; i < key_len; i++) {
        h ^= key[i] & 255;
        h *= 1111111111111111111;
    }
    return h ^ h>>32;
}

bool cflat__mem_equals(usize key_len, const byte lhs[key_len], const byte rhs[key_len]) {
    return memcmp(lhs, rhs, key_len) == 0;
}   

u64 cfalt__mask_step_next(u64 hash, u64 exponential, i32 last_index) {
    cflat_assert(exponential < 64);
    u64 mask = (1ULL << exponential) - 1;
    u64 step = (hash >> (64U - exponential)) | 1;
    return (last_index + step) & mask;
}

CflatHashTable* cflat_hashtable_new_opt(usize key_size, usize value_size, CflatArena *a, CflatHashTableNewOpt opt) {
    const usize capacity = next_pow2(opt.capacity);
    CflatHashTable *hashtable = cflat_arena_push(a, sizeof(*hashtable), .clear = true);
    hashtable->flags    = cflat_arena_push(a, sizeof(*hashtable->flags )*capacity, .align = cflat_alignof(uptr), .clear = true);
    hashtable->keys     = cflat_arena_push(a,                  key_size*capacity, .align = cflat_alignof(uptr), .clear = true);
    hashtable->values   = cflat_arena_push(a,                value_size*capacity, .align = cflat_alignof(uptr), .clear = true);
    hashtable->exponent = cflat_log2_u64(capacity);
    hashtable->count    = 0;
    hashtable->hash     = opt.hash;
    hashtable->equals   = opt.equals;
    return hashtable;
}

CflatHashTable* (cflat_hashtable_resize)(usize key_size, usize value_size, CflatArena *a, CflatHashTable *hashtable, usize capacity) {
    
    if (hashtable == NULL) {
        return cflat_hashtable_new_opt(key_size, value_size, a, (CflatHashTableNewOpt) { .capacity = capacity, .hash = hashtable->hash, .equals = hashtable->equals });
    }

    usize old_capacity = (1ULL << hashtable->exponent);
    usize new_capacity = next_pow2(capacity);
    usize old_size     = sizeof(*hashtable) + sizeof(*hashtable->flags)*old_capacity + key_size*old_capacity + value_size*old_capacity;
    usize new_size     = sizeof(*hashtable) + sizeof(*hashtable->flags)*new_capacity + key_size*new_capacity + value_size*new_capacity;
    usize word_aling   = cflat_alignof(uptr);

    CflatHashTable *new_hashtable = cflat_arena_extend(a, hashtable, old_size, new_size, .align = word_aling, .clear = true);

    CflatTempArena temp;
    arena_scratch_scope(temp, a) {
        if (new_hashtable == hashtable) {
            CflatHashTable *old_hashtable = cflat_hashtable_new_opt(key_size, value_size, temp.arena, (CflatHashTableNewOpt) { .capacity = old_capacity, .hash = hashtable->hash, .equals = hashtable->equals });
            cflat_mem_copy(old_hashtable->flags,  hashtable->flags,  sizeof(*hashtable->flags)*old_capacity);
            cflat_mem_copy(old_hashtable->keys,   hashtable->keys,   key_size*old_capacity);
            cflat_mem_copy(old_hashtable->values, hashtable->values, value_size*old_capacity);
            old_hashtable->count = hashtable->count;
            hashtable = old_hashtable;
        }
        
        cflat_arena_pop(a, old_size);
        new_hashtable = cflat_hashtable_new_opt(key_size, value_size, a, (CflatHashTableNewOpt) { .capacity = new_capacity, .hash = hashtable->hash, .equals = hashtable->equals });

        for (u64 i = 0; i < old_capacity; ++i) {
            void *old_key = hashtable->keys + i*key_size;
            void *old_value = hashtable->values + i*value_size;
            HashTableEntryFlag old_flags = hashtable->flags[i];
            u64 index;
            if (old_flags == CFLAT__HASH_ENTRY_USED) {
                cflat__hashtable_add_raw(key_size, value_size, new_hashtable, old_key, old_value, &index);
            }
        }
    }

    return new_hashtable;
}

bool (cflat_hashtable_add)(usize key_size, usize value_size, CflatArena *a, CflatHashTable **hashtable, const void *key, void *value) {
    usize index;
    void *ret = cflat__hashtable_add_raw(key_size, value_size, *hashtable, key, value, &index);
    if (ret == NULL) {
        *hashtable = (cflat_hashtable_resize)(key_size, value_size, a, *hashtable, 1ULL << ((*hashtable)->exponent + 1));
        ret = cflat__hashtable_add_raw(key_size, value_size, *hashtable, key, value, &index);
        cflat_assert(value != NULL);
    }
    return index != SIZE_MAX;
}

void* cflat__hashtable_add_raw(usize key_size, usize value_size, CflatHashTable *hashtable, const void *key, void *value, usize *out_index) {
    *out_index = SIZE_MAX;
    
    if (hashtable == NULL) {
        return NULL;
    }

    u64 hash = hashtable->hash ? hashtable->hash(key) : cflat__fnv1a_hash(key_size, key);
    for (u64 probe = hash;;) {
        probe = cfalt__mask_step_next(hash, hashtable->exponent, probe);
        switch (hashtable->flags[probe]) {
        case CFLAT__HASH_ENTRY_EMPTY: {
            const usize max_load = (3ULL << hashtable->exponent) / 4; // (2^n)3/4 aka 75% Full
            if (hashtable->count + 1 > max_load) { 
                return NULL;  
            }
            
            u64 index = *out_index = *out_index != SIZE_MAX ? *out_index : probe;
            hashtable->count++;
            hashtable->flags[index] = CFLAT__HASH_ENTRY_USED;
            cflat_mem_copy(hashtable->keys   + index*key_size,   key,   key_size  );
            cflat_mem_copy(hashtable->values + index*value_size, value, value_size);
            return value;
        }
        case CFLAT__HASH_ENTRY_DELETED: {
            *out_index = *out_index != SIZE_MAX ? *out_index : probe;
            break;
        }
        case CFLAT__HASH_ENTRY_USED: {
            void *hashtable_key = hashtable->keys + probe*key_size;
            bool equals = hashtable->equals ? hashtable->equals(hashtable_key, key) : cflat__mem_equals(key_size, hashtable_key, key);
            if (equals) {
                *out_index = SIZE_MAX;
                return hashtable->values + probe*value_size;
            }
            break;
        }
        default: cflat_assert(!"Invalid ENUM HashTableEntryFlag");
        }
    }
}

void* (cflat_hashtable_get)(usize key_size, usize value_size, CflatHashTable *hashtable, const void *key) {
    if (hashtable == NULL) {
        return NULL;
    }

    u64 hash = hashtable->hash ? hashtable->hash(key) : cflat__fnv1a_hash(key_size, key);
    for (u64 probe = hash;;) {
        probe = cfalt__mask_step_next(hash, hashtable->exponent, probe);
        switch (hashtable->flags[probe]) {
        case CFLAT__HASH_ENTRY_EMPTY: 
        case CFLAT__HASH_ENTRY_DELETED: {
            break;
        }
        case CFLAT__HASH_ENTRY_USED: {
            void *hashtable_key = hashtable->keys + probe*key_size;
            bool equals = hashtable->equals ? hashtable->equals(hashtable_key, key) : cflat__mem_equals(key_size, hashtable_key, key);
            if (equals) {
                return hashtable->values + probe*value_size;
            }
            break;
        }
        default: cflat_assert(!"Invalid ENUM HashTableEntryFlag");
        }
    }
}

#endif // CFLAT_IMPLEMENTATION

#if !defined(CFLAT_HASH_TABLE_NO_ALIAS)
#   define hashtable_new cflat_hashtable_new
#   define hashtable_add cflat_hashtable_add
#   define hashtable_get cflat_hashtable_get
#endif // CFLAT_HASH_TABLE_NO_ALIAS

#endif //CFLAT_HASH_SET_H