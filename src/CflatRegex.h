#ifndef CFLAT_CFLAT_REGEX_H
#define CFLAT_CFLAT_REGEX_H

#include "CflatCore.h"
#include "CflatArena.h"
#include "CflatSlice.h"
#include "CflatString.h"
#include "CflatAppend.h"
#include <iso646.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef CFLAT__SLICE_U32

#define CFLAT__SLICE_U32

typedef struct cflat_slice_u32 {
    CFLAT_SLICE_FIELDS(u32);
} CflatSliceU32;

#endif //CFLAT__SLICE_U32

#define cflat__nfa_epsilon UINT32_MAX

typedef struct cflat_adj_node_u32 {
    u32 u, v;
    struct cflat_adj_node_u32 *next; 
} CflatAdjNodeU32;

typedef struct cflat_adj_u32 {
    CFLAT_SLICE_FIELDS(CflatAdjNodeU32*);
} CflatAdjU32;

typedef struct cflat_nfa {
    CflatArena    *arena;
    CflatAdjU32   states;
    CflatSliceU32 groups;
    u32           start;
    bool          nullable;
} CflatNfa;

cflat_enum(CflatPatternOptions, u8) {
    CFLAT_MATCH_ONE            = 0x00,  //   Flag (1)
    CFLAT_MATCH_ZERO_OR_MORE   = 0x01,  // * Flag (1)
    CFLAT_MATCH_ONE_OR_MORE    = 0x02,  // + Flag (1)
    CFLAT_MATCH_ZERO_OR_ONE    = 0x04,  // ? Flag (1)
    CFLAT_CASE_SENSTIVE        = 0x00,  //   Flag (2)
    CFLAT_CASE_INSENSTIVE      = 0x10,  //   Flag (2)
};

cflat_enum(CflatMatchOptions, u8) {
    CFLAT_MATCH_ANYWHERE      = 0,
    CFLAT_MATCH_BEGIN         = 1,
    CFLAT_MATCH_END           = 2,
    CFLAT_MATCH_EXACT         = CFLAT_MATCH_BEGIN | CFLAT_MATCH_END,
};

CflatNfa cflat_nfa_new(CflatArena *arena);
// Capture Group
void cflat_nfa_begin_group(CflatNfa *nfa);
void cflat_nfa_match_opt(CflatNfa *nfa, CflatStringView pattern, CflatPatternOptions opt);
// (Previous pattern or "") | pattern 
void cflat_nfa_or_opt(CflatNfa *nfa, CflatStringView pattern, CflatPatternOptions opt);
void cflat_nfa_end_group_opt(CflatNfa *nfa, CflatPatternOptions opt);

CflatStringView cflat_nfa_matches(CflatNfa nfa, CflatStringView text);
bool cflat_nfa_match_next(CflatNfa nfa, CflatStringView *match, const CflatStringView text); 

#endif //CFLAT_CFLAT_REGEX_H

#ifdef CFLAT_IMPLEMENTATION

CflatNfa cflat_nfa_new(CflatArena *arena) {
    return (CflatNfa) { .arena = arena };
}

static void cflat__nfa_add_transition(CflatArena *arena, CflatAdjNodeU32 **stack, u32 match, u32 dst) {
    CflatAdjNodeU32 *node = cflat_arena_push_struct(CflatAdjNodeU32, arena);
    node->u = match;
    node->v = dst;
    cflat_ll_push(*stack, node, next);
}

static char cflat__flip_case(char c) {
    char f = 0;
    if (c >= 'a' && c <= 'z')      f = c - 32;
    else if (c >= 'A' && c <= 'Z') f = c + 32;
    return f;
}

void cflat_nfa_match_opt(CflatNfa *nfa, CflatStringView pattern, CflatPatternOptions opt) {
    CflatArena *arena = nfa->arena;

    if (nfa->states.length == 0) {
        cflat_slice_resize(arena, &nfa->states, pattern.length);
        nfa->states.length = 1;
    }

    u32 start_state   = (u32)nfa->states.length - 1;
    u32 end_state     = start_state + (u32)pattern.length;
    
    bool match_any  = (opt & CFLAT_MATCH_ZERO_OR_MORE);
    bool match_0or1 = (opt & CFLAT_MATCH_ZERO_OR_ONE );
    bool match_many = (opt & CFLAT_MATCH_ONE_OR_MORE );
    bool match_one  = (opt & CFLAT_MATCH_ZERO_OR_ONE );
    bool case_ins   = (opt & CFLAT_CASE_INSENSTIVE   );

    if (match_any || match_one) {
        cflat__nfa_add_transition(arena, &nfa->states.data[start_state], cflat__nfa_epsilon, end_state);
    }

    for (usize i = 0; i < pattern.length; ++i) {
        u32 q = start_state + (u32)i;
        u8  c = (u8)pattern.data[i];
        CflatAdjNodeU32 **head = &nfa->states.data[q];
        
        cflat__nfa_add_transition(arena, head, c, q + 1);
        if (case_ins) {
            u8 f = cflat__flip_case(c);
            if (f != 0 && f != c) {
                cflat__nfa_add_transition(arena, head, f, q + 1);
            }
        }
    }

    if (match_any || match_many) {
        cflat__nfa_add_transition(arena, &nfa->states.data[end_state], cflat__nfa_epsilon, start_state);
    }

    if (pattern.length == 0 || match_any || match_0or1) {
        nfa->nullable = true;
    }

    nfa->states.length = end_state + 1;
}

static void cflat__nfa_expand_stack(CflatNfa nfa, u8 *states, u32 *epsilons, usize *epsilon_index) {
    while (*epsilon_index > 0) {
        u32 s = epsilons[--(*epsilon_index)];
        for (CflatAdjNodeU32 *t = nfa.states.data[s]; t != NULL; t = t->next) {
            if (t->u == cflat__nfa_epsilon) {
                u32 target = t->v;
                if (!states[target]) {
                    states[target] = 1;
                    epsilons[(*epsilon_index)++] = target;
                }
            }
        }
    }
}

void cflat_nfa_begin_group(CflatNfa *nfa) {
    u32 entry = (nfa->states.length == 0) ? 0 : (u32)nfa->states.length - 1;

    cflat_slice_append(nfa->arena, &nfa->groups, entry);
}

void cflat_nfa_end_group_opt(CflatNfa *nfa, CflatPatternOptions opt) {
    if (nfa->groups.length == 0 || nfa->states.length == 0) {
        return;
    }

    bool is_root_group = (nfa->groups.length == 1);
    u32 start = nfa->groups.data[nfa->groups.length - 1];
    u32 end = (u32)nfa->states.length - 1;

    bool match_any  = (opt & CFLAT_MATCH_ZERO_OR_MORE);
    bool match_0or1 = (opt & CFLAT_MATCH_ZERO_OR_ONE );
    bool match_many = (opt & CFLAT_MATCH_ONE_OR_MORE );
    bool match_one  = (opt == CFLAT_MATCH_ONE);

    if (match_any || match_0or1 || match_one) {
        cflat__nfa_add_transition(nfa->arena, &nfa->states.data[start], cflat__nfa_epsilon, end);
    }

    if (match_any || match_many) {
        cflat__nfa_add_transition(nfa->arena, &nfa->states.data[end], cflat__nfa_epsilon, start);
    }

    if (match_any || match_0or1) {
        nfa->nullable = true;
    }

    if (is_root_group) {
        nfa->start = start;
    }

    nfa->groups.length--;
}

void cflat_nfa_or_opt(CflatNfa *nfa, CflatStringView pattern, CflatPatternOptions opt) {
    CflatArena *arena = nfa->arena;
    
    u32 anchor = nfa->start; 
    if (nfa->groups.length > 0) {
        anchor = nfa->groups.data[nfa->groups.length - 1];
    }

    u32 lhs_end = (u32)nfa->states.length - 1;
    u32 fork_state = (u32)nfa->states.length;
    cflat_slice_resize(arena, &nfa->states, nfa->states.length + 1);
    nfa->states.length++;
    
    cflat__nfa_add_transition(arena, &nfa->states.data[fork_state], cflat__nfa_epsilon, anchor);

    u32 rhs_start = (u32)nfa->states.length;
    cflat_nfa_match_opt(nfa, pattern, opt);
    u32 rhs_end = (u32)nfa->states.length - 1;

    cflat__nfa_add_transition(arena, &nfa->states.data[fork_state], cflat__nfa_epsilon, rhs_start);

    u32 join_state = (u32)nfa->states.length;
    cflat_slice_resize(arena, &nfa->states, nfa->states.length + 1);
    nfa->states.length++;

    cflat__nfa_add_transition(arena, &nfa->states.data[lhs_end], cflat__nfa_epsilon, join_state);
    cflat__nfa_add_transition(arena, &nfa->states.data[rhs_end], cflat__nfa_epsilon, join_state);

    if (nfa->groups.length > 0) {
        nfa->groups.data[nfa->groups.length - 1] = fork_state;
    } else {
        nfa->start = fork_state;
    }
}

CflatStringView cflat_nfa_matches(CflatNfa nfa, CflatStringView text) {
    CflatStringView result = {0};
    
    if (nfa.states.length == 0) 
        return result;

    CflatTempArena temp;
    cflat_scratch_arena_scope(temp, 1, &nfa.arena) {
        u8 *curr_states    = cflat_arena_push_array(u8,    temp.arena, nfa.states.length);
        u8 *next_states    = cflat_arena_push_array(u8,    temp.arena, nfa.states.length);
        char **curr_starts = cflat_arena_push_array(char*, temp.arena, nfa.states.length);
        char **next_starts = cflat_arena_push_array(char*, temp.arena, nfa.states.length);
        u32 *epsilons      = cflat_arena_push_array(u32,   temp.arena, nfa.states.length);
        
        u32 last_index = (u32)nfa.states.length - 1;

        for (usize start_index = 0; start_index <= text.length; ++start_index) {
            cflat_mem_zero(curr_states, nfa.states.length);
            usize epsilon_index = 0;

            curr_states[nfa.start]    = 1;
            curr_starts[nfa.start]    = (char*)text.data + start_index;
            epsilons[epsilon_index++] = nfa.start;
            
            cflat__nfa_expand_stack(nfa, curr_states, epsilons, &epsilon_index);

            if (curr_states[last_index] && nfa.nullable) {
                result.data = (char*)text.data + start_index;
                result.length = 0;
            }

            for (usize i = start_index; i < text.length; ++i) {
                cflat_mem_zero(next_states, nfa.states.length);
                epsilon_index = 0;
                u8 c = (u8)text.data[i];

                for (u32 s = 0; s < (u32)nfa.states.length; ++s) {
                    if (!curr_states[s]) continue;

                    for (CflatAdjNodeU32 *t = nfa.states.data[s]; t != NULL; t = t->next) {
                        if (t->u == c) {
                            if (!next_states[t->v]) {
                                next_states[t->v] = 1;
                                next_starts[t->v] = curr_starts[s];
                                epsilons[epsilon_index++] = t->v;
                            }
                        }
                    }
                }

                if (epsilon_index == 0) break;

                cflat__nfa_expand_stack(nfa, next_states, epsilons, &epsilon_index);
                
                cflat_swap(u8*,    curr_states, next_states);
                cflat_swap(char**, curr_starts, next_starts);

                if (curr_states[last_index]) {
                    usize curr_len = (i + 1) - start_index;
                    if (result.data == NULL || curr_len >= result.length) {
                        result.data = (char*)text.data + start_index;
                        result.length = curr_len;
                    }
                }
            }

            if (result.data != NULL) 
                break;
        }
    }

    return result;
}

bool cflat_nfa_match_next(CflatNfa nfa, CflatStringView *match, const CflatStringView text) {
    usize start_offset = 0;
    
    if (match->data != NULL) {
        start_offset = (usize)(match->data - text.data) + (match->length > 0 ? match->length : 1);
    }

    for (usize i = start_offset; i <= text.length; ++i) {
        CflatStringView remaining = {
            .data = text.data + i,
            .length = text.length - i
        };
        
        CflatStringView result = cflat_nfa_matches(nfa, remaining);
        if (result.data != NULL) {
            *match = result;
            return true;
        }
    }
    
    return false;
}

#endif //CFLAT_IMPLEMENTATION
