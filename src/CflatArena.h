#ifndef CFLAT_ARENA_H
#define CFLAT_ARENA_H
#include <stdbool.h>

#include "CflatCore.h"

#define cflat_stackalloc(SIZE) ((void*)((byte[(SIZE)]){0}))
#define cflat_lit(LITERAL) (*((typeof(LITERAL)[1]) {LITERAL}))

typedef struct cflat_arena {
    struct cflat_arena_node *curr;
    struct cflat_arena_node *free;
    usize pos;
} CflatArena;

typedef struct cflat_arena_node {
    CflatArena _arena;
    struct cflat_arena_node *prev;
    void* ptr2free;
    usize len;
    usize cap;
    byte data[];
} CflatArenaNode;

typedef struct cflat_arena_scope
{
    CflatArena *arena;
    usize pos;
} CflatArenaScope;

CFLAT_DEF CflatArena* cflat_arena_init(void *mem, usize size);
CFLAT_DEF CflatArena* cflat_arena_new(usize size_hint);

typedef struct cflat_alloc_opt {
    usize align;
    bool clear;
} CflatAllocOpt;

CFLAT_DEF void* cflat_arena_push_opt(CflatArena *arena, usize size, CflatAllocOpt opt);

#define cflat_arena_push(a, size, ...) cflat_arena_push_opt((a), (size), (CflatAllocOpt) {__VA_ARGS__})

CFLAT_DEF void cflat_arena_pop(CflatArena *arena, usize size);
CFLAT_DEF void cflat_arena_set_pos(CflatArena *arena, usize pos);
CFLAT_DEF void cflat_arena_clear(CflatArena *arena);
CFLAT_DEF void cflat_arena_delete(CflatArena *arena);

#define cflat_define_padded_struct(type, size) union {      \
    byte raw[sizeof(type) + (size)];                        \
    type structure;                                         \
}

#define cflat_padded_struct(type, size) &(cflat_define_padded_struct(type, size)){0}.structure

CFLAT_DEF CflatArenaScope cflat_arena_scope_begin(CflatArena *arena);
CFLAT_DEF void cflat_arena_scope_end(CflatArenaScope temp_arena);

CFLAT_DEF CflatArena* cflat_rent_arena();
CFLAT_DEF void cflat_return_arena(CflatArena *arena);

#define cflat_arena_scope_defer(arena) \
    cflat_defer(CflatArenaScope _scope = cflat_arena_scope_begin((arena)), cflat_arena_scope_end(_scope))

#define cflat_arena_rent_defer(arena) \
    cflat_defer(arena = cflat_rent_arena(), cflat_return_arena(arena))

#if defined(CFLAT_ARENA_IMPLEMENTATION)

static void* cflat__os_align_malloc(usize align, usize size) {
    #if defined(OS_WINDOWS)
    return _aligned_malloc(size, align);
    #elif defined(OS_UNIX)
    return aligned_alloc(align, size);
    #elif
    #error Unsuported platform
    #endif
}

static void cflat__os_align_free(void *memory) {
    #if defined(OS_WINDOWS)
    _aligned_free(memory);
    #elif defined(OS_UNIX)
    free(memory);
    #elif
    #error Unsuported platform
    #endif
}

static void cflat__node_init(CflatArenaNode *node, void* ptr2free, const usize size) {
    *node = (CflatArenaNode) {
        ._arena = {
            .curr = node,
            .free = NULL,
            .pos = 0,
        },
        .len = sizeof(CflatArenaNode),
        .cap = size,
        .ptr2free = ptr2free,
    };
}

CflatArena* cflat_arena_init(void *mem, const usize size) {
    cflat__node_init(mem, NULL, size);
    return &((CflatArenaNode*)mem)->_arena;
}

CflatArena* cflat_arena_new(const usize size_hint) {
    const usize size = cflat_align_pow2(sizeof(CflatArenaNode) + size_hint, alignof(uptr));
    CflatArenaNode* node = cflat__os_align_malloc(alignof(uptr), size);
    cflat_assert(node && "Bad alloc!");
    cflat__node_init(node, (void*)node, size);
    return &node->_arena;
}

void cflat_arena_delete(CflatArena *arena) {
    if (arena == NULL) return;

    CflatArenaNode *current = arena->curr;
    CflatArenaNode *freelst = arena->free;
    CflatArenaNode *it;
    cflat_mem_zero(arena, sizeof *arena);

    for (it = current; it;) {
        CflatArenaNode *prev = it->prev;
        void *ptr2free = it->ptr2free;
        if (ptr2free) {
            cflat__os_align_free(ptr2free);
        }
        it = prev;
    }

    for (it = freelst; it;) {
        CflatArenaNode *prev = it->prev;
        void *ptr2free = it->ptr2free;
        if (ptr2free) {
            cflat__os_align_free(ptr2free);
        }
        it = prev;
    }

}

void* cflat_arena_push_opt(CflatArena *arena, const usize size, struct cflat_alloc_opt opt) {

    cflat_assert(arena != NULL);
    opt.align = cflat_max(opt.align, 1);

    CflatArenaNode *node = arena->curr;
    if (node == NULL) {
        node = arena->free;
        for(CflatArenaNode *prev = NULL; node; prev = node, node = node->prev) {
            node->len = sizeof(struct cflat_arena_node);
            const uptr alloc_end = cflat_align_pow2(node->len, opt.align) + size;
            if (alloc_end <= node->cap) {
                if(prev) prev->prev = node->prev;
                else arena->free = node->prev;
                cflat_ll_push(arena->curr, (node), prev);
                break;
            }
        }
        if (node == NULL) node = cflat_arena_new(size)->curr;
        arena->curr = node;
    }

    uptr pre_len = cflat_align_pow2(node->len, opt.align);
    uptr pst_len = pre_len + size;

    if (pst_len > node->cap) {
        CflatArenaNode *new_node = cflat_arena_new(pst_len)->curr;

        new_node->prev = node;
        pre_len = cflat_align_pow2(new_node->len, opt.align);
        pst_len = pre_len + size;

        node = new_node;
        arena->curr = node;
    }

    byte *mem = (byte*)node + pre_len;
    if (opt.clear) cflat_mem_zero(mem, size);

    node->len = pst_len;
    arena->pos += pst_len;
    return mem;
}

void cflat_arena_pop(CflatArena *arena, usize size) {
    if (size == 0) return;

    CflatArenaNode *node = arena->curr;
    while (node) {
        CflatArenaNode *prev = node->prev;

        if (size <= node->len - sizeof(CflatArenaNode)) {
            node->len -= size;
            arena->pos -= size;
            break;
        }

        const usize allocated = node->len - sizeof(CflatArenaNode);
        size -= allocated;

        cflat_ll_push(arena->free, node, prev);
        arena->curr = prev;

        node = prev;
    }
}

void cflat_arena_set_pos(CflatArena *arena, const usize pos) {
    if (arena->pos >= pos) {
        cflat_arena_pop(arena, arena->pos - pos);
        return;
    }

    cflat_arena_push_opt(arena, pos, (CflatAllocOpt) {
        .align = 1,
        .clear = false
    });
}

void cflat_arena_clear(CflatArena *arena) {
    CflatArenaNode *freelst = arena->free;
    arena->free = arena->curr;
    arena->curr = NULL;
    if (freelst) {
        freelst->prev = arena->free;
        arena->free = freelst;
    }
    arena->pos = 0;
}

CflatArenaScope cflat_arena_scope_begin(CflatArena *arena) {
    return (CflatArenaScope) {
        .arena = arena,
        .pos = arena->pos
    };
}

void cflat_arena_scope_end(const CflatArenaScope temp_arena) {
    cflat_arena_set_pos(temp_arena.arena, temp_arena.pos);
}

#define CFLAT__TEMP_ARENA_INITIAL_CAPACITY KiB(64)

cflat_thread_local static byte cflat__tls_arena_pool_bytes[CFLAT__TEMP_ARENA_INITIAL_CAPACITY];
cflat_thread_local static CflatArena *cflat__tls_arena_pool = NULL;

CflatArena* cflat_rent_arena() {
    if (cflat__tls_arena_pool == NULL)
        return cflat__tls_arena_pool = cflat_arena_init(cflat__tls_arena_pool_bytes, CFLAT__TEMP_ARENA_INITIAL_CAPACITY);

    if (cflat__tls_arena_pool->free == NULL)
        return cflat_arena_new(CFLAT__TEMP_ARENA_INITIAL_CAPACITY);

    CflatArenaNode *node = cflat__tls_arena_pool->free;
    cflat_ll_pop(cflat__tls_arena_pool->free, prev);
    node->len = sizeof(CflatArenaNode);
    node->prev = NULL;
    node->_arena = (CflatArena) {
        .curr = node,
        .free = NULL,
    };
    return &node->_arena;
}

void cflat_return_arena(CflatArena *arena) {
    if (cflat__tls_arena_pool == NULL)
        cflat__tls_arena_pool = cflat_arena_init(cflat__tls_arena_pool_bytes, CFLAT__TEMP_ARENA_INITIAL_CAPACITY);

    if (cflat__tls_arena_pool == arena) {
        cflat_arena_clear(arena);
        return;
    }

    for (CflatArenaNode *it = arena->free; it; ) {
        CflatArenaNode *prev = it->prev;
        cflat_ll_push(cflat__tls_arena_pool->free, (it), prev);
        it = prev;
    }

    for (CflatArenaNode *it = arena->curr; it; ) {
        CflatArenaNode *prev = it->prev;
        cflat_ll_push(cflat__tls_arena_pool->free, (it), prev);
        it = prev;
    }
}

#endif //CFLAT_ARENA_IMPLEMENTATION

#ifndef CFLAT_ARENA_NO_ALIAS

#   define stackalloc cflat_stackalloc
#   define mem_copy cflat_mem_copy
#   define clit cflat_lit
#   define arena_init cflat_arena_init
#   define arena_new cflat_arena_new
#   define arena_push cflat_arena_push
#   define arena_pop cflat_arena_pop
#   define arena_clear cflat_arena_clear
#   define arena_set_pos cflat_arena_set_pos
#   define arena_delete cflat_arena_delete
#   define rent_arena cflat_rent_arena
#   define return_arena cflat_return_arena
#   define arena_scope_defer cflat_arena_scope_defer
#   define arena_rent_defer cflat_arena_rent_defer
#   define Arena CflatArena
#   define ArenaScope CflatArenaScope

#endif // CFLAT_ARENA_NO_ALIAS

#endif //CFLAT_ARENA_H
