#ifndef CFLAT_ARENA_H
#define CFLAT_ARENA_H
#include <assert.h>
#include <stdbool.h>
#include "CflatCore.h"

#define CFLAT_ARENA_HEADER (sizeof(CflatArenaNode))
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
    usize pos;
    usize res;
    usize cmt;
    byte data[];
} CflatArenaNode;

typedef struct cflat_arena_scope
{
    CflatArena *arena;
    usize pos;
} CflatTempArena;

CFLAT_DEF CflatArena* cflat_arena_init(void *mem, usize size);
CFLAT_DEF CflatArena* cflat_arena_new(usize reserve, usize commit);

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

CFLAT_DEF CflatTempArena cflat_arena_temp_begin(CflatArena *arena);
CFLAT_DEF void cflat_arena_temp_end(CflatTempArena temp_arena);

#define cflat_temp_arena_scope(arena)\
    cflat_defer(TempArena CONCAT(_t, __LINE__) = cflat_arena_temp_begin((arena)), cflat_arena_temp_end(CONCAT(_t, __LINE__)))

cflat_thread_local CflatArena *cflat__tls_scratches[2];

CFLAT_DEF CflatTempArena cflat_get_scratch_arena_pool(CflatArena **pool, usize pool_len, CflatArena **conflicts, usize count);

// ... (conflicts): Arena*[] | null
#define cflat_get_scratch_arena(...) cflat_get_scratch_arena_pool(                                  \
    cflat__tls_scratches,                                                                           \
    CFLAT_ARRAY_SIZE(cflat__tls_scratches),                                                       \
    ((CflatArena*[]){__VA_ARGS__}),                                                                 \
    CFLAT_ARRAY_SIZE(((CflatArena*[]){__VA_ARGS__}))                                              \
)


// tmp: TempArena
// ... (conflicts): Arena*[] | null
#define cflat_scratch_arena_scope(tmp, ...)\
    cflat_defer(tmp = cflat_get_scratch_arena(__VA_ARGS__), cflat_arena_temp_end((tmp)))

#if defined(CFLAT_IMPLEMENTATION)

#if ASAN_ENABLED
#include <sanitizer/asan_interface.h>
#include <sanitizer/lsan_interface.h>
#else
# define ASAN_POISON_MEMORY_REGION(addr, size)   ((void)(addr), (void)(size))
# define ASAN_UNPOISON_MEMORY_REGION(addr, size) ((void)(addr), (void)(size))
#endif

#if VALGRIND_ENABLED
#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>
#else
# define VALGRIND_MALLOCLIKE_BLOCK(addr, size, rzB, is_zeroed) ((void)(addr), (void)(size), (void)(rzB), (void)(is_zeroed))
# define VALGRIND_FREELIKE_BLOCK(addr, size) ((void)(addr), (void)(size))
#endif

#if defined(OS_WINDOWS)
#include <memoryapi.h>
#elif defined(OS_UNIX)
#include <sys/mman.h>
#endif

static void *cflat__os_reserve(usize size)
{
    #if defined(OS_WINDOWS)
    void *result = VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
    return result;
    #elif defined(OS_UNIX)
    void *result = mmap(0, size, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(result == MAP_FAILED)
    {
        result = 0;
    }
    return result;
    #elif
    #error Unsuported platform
    #endif
}

static bool cflat__os_commit(void *ptr, usize size)
{
    #if defined(OS_WINDOWS)
    const bool result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0;
    return result;
    #elif defined(OS_UNIX)
    mprotect(ptr, size, PROT_READ|PROT_WRITE);
    return 1;
    #elif
    #error Unsuported platform
    #endif
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void cflat__os_decommit(void *ptr, usize size)
{
    #if defined(OS_WINDOWS)
    VirtualFree(ptr, size, MEM_DECOMMIT);
    #elif defined(OS_UNIX)
    madvise(ptr, size, MADV_DONTNEED);
    mprotect(ptr, size, PROT_NONE);
    #elif
    #error Unsuported platform
    #endif
}
#pragma GCC diagnostic pop

static void cflat__os_release(void *ptr, const usize size)
{
    #if defined(OS_WINDOWS)
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
    #elif defined(OS_UNIX)
    munmap(ptr, size);
    #elif
    #error Unsuported platform
    #endif
}

static void cflat__node_init(CflatArenaNode *node, void* ptr2free, const usize res, const usize cmt) {
    *node = (CflatArenaNode) {
        ._arena = {
            .curr = node,
            .free = NULL,
            .pos = 0,
        },
        .pos = sizeof(CflatArenaNode),
        .res = res,
        .cmt = cmt,
        .ptr2free = ptr2free,
    };
}

#define cflat__arena_node_poison_unused(node) \
    ASAN_POISON_MEMORY_REGION((node)->data, (node)->res - (node)->pos)

CflatArena* cflat_arena_init(void *mem, const usize size) {
    cflat__node_init(mem, NULL, size, size);
    return &((CflatArenaNode*)mem)->_arena;
}

CflatArena* cflat_arena_new(const usize reserve, const usize commit) {

    cflat_assert(reserve >= commit);

    const usize page_size = 4096;
    const usize reserve_size = cflat_align_pow2(reserve, page_size);
    const usize commit_size = cflat_align_pow2(commit, page_size);

    CflatArenaNode* node = cflat__os_reserve(reserve_size);
    VALGRIND_MALLOCLIKE_BLOCK(node, reserve_size, 0, false);

    cflat__os_commit(node, commit_size);
    cflat_assert(node && "Bad alloc!");

    cflat__node_init(node, (void*)node, reserve_size, commit_size);
    cflat__arena_node_poison_unused(node);
    return &node->_arena;
}

void cflat_arena_delete(CflatArena *arena) {
    if (arena == NULL) return;

    CflatArenaNode *current = arena->curr;
    CflatArenaNode *node = arena->free;
    CflatArenaNode *it;
    cflat_mem_zero(arena, sizeof *arena);

    for (it = current; it;) {
        CflatArenaNode *prev = it->prev;
        void *ptr2free = it->ptr2free;
        if (ptr2free) {
            const usize res = it->res;
            ASAN_UNPOISON_MEMORY_REGION(ptr2free, res);
            cflat__os_release(ptr2free, res);
            VALGRIND_FREELIKE_BLOCK(ptr2free, 0);
        }
        it = prev;
    }

    for (it = node; it;) {
        CflatArenaNode *prev = it->prev;
        void *ptr2free = it->ptr2free;
        if (ptr2free) {
            const usize res = it->res;
            ASAN_UNPOISON_MEMORY_REGION(ptr2free, res);
            cflat__os_release(ptr2free, res);
            VALGRIND_FREELIKE_BLOCK(ptr2free, 0);
        }
        it = prev;
    }

}

void* cflat_arena_push_opt(CflatArena *arena, const usize size, CflatAllocOpt opt) {

    cflat_assert(arena != NULL);
    if(opt.align == 0) opt.align = cflat_alignof(void*);

    CflatArenaNode *current_node = arena->curr;
    uptr pre = cflat_align_pow2(current_node->pos, opt.align);
    uptr pst = pre + size;

    if (current_node->res < pst) {
        CflatArenaNode *new_node = arena->free;

        for(CflatArenaNode *prev_node = NULL; new_node; prev_node = new_node, new_node = new_node->prev) {
            new_node->pos = sizeof(CflatArenaNode);

            if (new_node->res >= cflat_align_pow2(new_node->pos, opt.align) + size) {
                if(prev_node) prev_node->prev = new_node->prev;
                else arena->free = new_node->prev;
                break;
            }
        }

        if (new_node == NULL)
        {
            usize res = current_node->res;
            usize cmt = current_node->cmt;
            if(size + sizeof(CflatArenaNode) > res)
            {
                res = cflat_align_pow2(size + sizeof(CflatArenaNode), opt.align);
                cmt = cflat_align_pow2(size + sizeof(CflatArenaNode), opt.align);
            }
            new_node = cflat_arena_new(res, cmt)->curr;
        }

        cflat_ll_push(arena->curr, new_node, prev);

        current_node = new_node;
        pre = cflat_align_pow2(current_node->pos, opt.align);
        pst = pre + size;
    }

    if(current_node->cmt < pst) {
        usize commit_aligned_to_pst = pst + current_node->cmt - 1;
        commit_aligned_to_pst -= commit_aligned_to_pst%current_node->cmt;

        const usize commit_pst_clamped = cflat_min(commit_aligned_to_pst, current_node->res);
        const usize cmt_size = commit_pst_clamped - current_node->cmt;

        byte *commited_ptr = (byte *)current_node + current_node->cmt;
        cflat__os_commit(commited_ptr, cmt_size);
        current_node->cmt = commit_pst_clamped;
    }

    void *result = NULL;

    if (current_node->cmt >= pst) {
        result = (byte *)current_node + pre;
        current_node->pos = pst;
        ASAN_UNPOISON_MEMORY_REGION(result, size);
        if (opt.clear) cflat_mem_zero(result, size);
        arena->pos += pst - current_node->pos;
    }

    return result;
}

void cflat_arena_pop(CflatArena *arena, usize size) {
    if (size == 0) return;
    arena->pos -= size;

    CflatArenaNode *curr = arena->curr;
    while (curr) {
        CflatArenaNode *prev = curr->prev;
        const usize allocated = curr->pos - sizeof *curr;
        if (size <= allocated) {
            curr->pos -= size;
            cflat__arena_node_poison_unused(curr);
            break;
        }
        size -= allocated;
        curr->pos = sizeof *curr;
        cflat__arena_node_poison_unused(curr);
        cflat_ll_push(arena->free, curr, prev);
        arena->curr = prev;
        curr = prev;
    }

    if (arena->curr == NULL) {
        curr = arena->free;
        curr->pos = sizeof *curr;
        arena->free = curr->prev;
        cflat_ll_push(arena->curr, curr, prev);
        arena->curr = curr;
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

    #if defined(ASAN_ENABLED)
    for (const CflatArenaNode *curr = arena->curr; curr; curr = curr->prev) {
        cflat__arena_node_poison_unused(curr);
    }
    #endif

    arena->pos = 0;
    CflatArenaNode *freelst = arena->free;
    arena->free = arena->curr;
    if (freelst) {
        freelst->prev = arena->free;
        arena->free = freelst;
    }
    arena->curr = NULL;
    freelst = arena->free;
    freelst->pos = sizeof *freelst;
    arena->free = freelst->prev;
    cflat_ll_push(arena->curr, freelst, prev);
    arena->curr = freelst;
}

CflatTempArena cflat_arena_temp_begin(CflatArena *arena) {
    return (CflatTempArena) {
        .arena = arena,
        .pos = arena->pos
    };
}

void cflat_arena_temp_end(const CflatTempArena temp_arena) {
    cflat_arena_set_pos(temp_arena.arena, temp_arena.pos);
}

cflat_thread_local static byte cflat__tls_arena_bytes[2][KiB(64)];

CflatTempArena cflat_get_scratch_arena_pool(CflatArena **pool, const usize pool_len, CflatArena **conflicts, const usize count) {
    CflatArena *result = NULL;
    CflatArena **arena_ptr = pool;
    for(u64 i = 0; i < pool_len; i += 1, arena_ptr += 1)
    {
        if (*arena_ptr == NULL || (*arena_ptr)->curr == NULL) {
            *arena_ptr = cflat_arena_init(
                cflat__tls_arena_bytes[i],
                sizeof(cflat__tls_arena_bytes[i])
            );
        }

        CflatArena **conflict_ptr = conflicts;
        bool has_conflict = false;
        for(u64 j = 0; j < count; j += 1, conflict_ptr += 1)
        {
            if(*arena_ptr == *conflict_ptr)
            {
                has_conflict = 1;
                break;
            }
        }
        if(!has_conflict)
        {
            result = *arena_ptr;
            break;
        }
    }

    return cflat_arena_temp_begin(result);
}

#endif //CFLAT_IMPLEMENTATION

#ifndef CFLAT_ARENA_NO_ALIAS

#   define stackalloc cflat_stackalloc
#   define mem_copy cflat_mem_copy
#   define clit cflat_lit
#   define os_aligned_malloc cflat_os_aligned_malloc
#   define os_aligned_free cflat_os_aligned_free
#   define arena_init cflat_arena_init
#   define arena_new cflat_arena_new
#   define arena_push cflat_arena_push
#   define arena_pop cflat_arena_pop
#   define arena_clear cflat_arena_clear
#   define arena_set_pos cflat_arena_set_pos
#   define arena_delete cflat_arena_delete
#   define get_scratch_arena cflat_get_scratch_arena
#   define scratch_arena_scope cflat_scratch_arena_scope
#   define temp_arena_scope cflat_temp_arena_scope
#   define arena_temp_begin cflat_arena_temp_begin
#   define arena_temp_end cflat_arena_temp_end
#   define Arena CflatArena
#   define TempArena CflatTempArena

#endif // CFLAT_ARENA_NO_ALIAS

#endif //CFLAT_ARENA_H
