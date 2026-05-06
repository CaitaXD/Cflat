#ifndef CFLAT_CFLAT_REGEX_H
#define CFLAT_CFLAT_REGEX_H

#include "CflatCore.h"
#include "CflatArena.h"
#include "CflatSlice.h"
#include "CflatString.h"
#include "CflatAppend.h"
#include <limits.h>
#include <stdio.h>

#define cflat__nfa_epsilon (0)

typedef struct cflat_adj_u32 {
    u32 u;             
    u32 v;
    struct cflat_adj_u32 *next; 
} CflatAdjU32;

typedef struct cflat_nfa {
    CflatArena   *arena;
    usize states_count;
    CflatAdjU32 **states;
} CflatNfa;

typedef struct cflat_state_slice {
    CFLAT_SLICE_FIELDS(u8);
} CflatStaceSlice;

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

#endif

CflatNfa cflat_nfa_new(CflatArena *arena);
// Capture Group
void cflat_nfa_begin_group(CflatNfa *nfa);
void cflat_nfa_match_opt(CflatNfa *nfa, CflatStringView pattern, CflatPatternOptions opt);
// (Previous pattern or "") | pattern 
void cflat_nfa_or_opt(CflatNfa *nfa, CflatStringView pattern, CflatPatternOptions opt);
void cflat_nfa_end_group_opt(CflatNfa *nfa, CflatMatchOptions opt);

CflatStringView cflat_nfa_matches(CflatNfa nfa, CflatStringView text);
bool cflat_nfa_match_next(CflatNfa nfa, CflatStringView *match, const CflatStringView text); 

#ifdef CFLAT_IMPLEMENTATION

CflatNfa cflat_nfa_new(CflatArena *arena) {
    return (CflatNfa) {
        .arena = arena
    };
}

static void cflat__nfa_trnasition(CflatNfa *nfa, u32 from, u32 match, u32 to) {
    CflatAdjU32 *node = cflat_arena_push(nfa->arena, sizeof(*node));
    node->u = match;
    node->v = to;
    cflat_ll_push(nfa->states[from], node, next);
}

static char cflat__flip_case(char c) {
    char f = 0;
    if (c >= 'a' && c <= 'z')      f = c - 32;
    else if (c >= 'A' && c <= 'Z') f = c + 32;
    return f;
}

void cflat_nfa_match_opt(CflatNfa *nfa, CflatStringView pattern, CflatPatternOptions opt) {
    if (nfa->states_count == 0) {
        nfa->states = cflat_arena_push(nfa->arena, sizeof(CflatAdjU32*) * 1024);
        cflat_mem_zero(nfa->states, sizeof(CflatAdjU32*) * 1024);
        nfa->states_count = 1;
    }

    u32 start_state   = (u32)nfa->states_count - 1;
    usize pattern_len = pattern.length;
    u32 end_state     = start_state + (u32)pattern_len;
    
    bool match_any  = (opt & CFLAT_MATCH_ZERO_OR_MORE);
    bool match_many = (opt & CFLAT_MATCH_ONE_OR_MORE );
    bool match_one  = (opt & CFLAT_MATCH_ZERO_OR_ONE );
    bool case_ins   = (opt & CFLAT_CASE_INSENSTIVE   );

    if (match_any || match_one) {
        cflat__nfa_trnasition(nfa, start_state, cflat__nfa_epsilon, end_state);
    }

    for (usize i = 0; i < pattern_len; ++i) {
        u32 q = start_state + (u32)i;
        u8  c = (u8)pattern.data[i];
        
        cflat__nfa_trnasition(nfa, q, c, q + 1);

        if (case_ins) {
            u8 f = cflat__flip_case(c);
            if (f != 0 && f != c) {
                cflat__nfa_trnasition(nfa, q, f, q + 1);
            }
        }
    }

    if (match_any || match_many) {
        cflat__nfa_trnasition(nfa, end_state, cflat__nfa_epsilon, start_state);
    }

    nfa->states_count = end_state + 1;
}

static void cflat__nfa_expand_stack(CflatNfa nfa, u8 *states, u32 *epsilons, usize *curr_epsilon) {
    while (*curr_epsilon > 0) {
        u32 s = epsilons[--(*curr_epsilon)];
        for (CflatAdjU32 *t = nfa.states[s]; t != NULL; t = t->next) {
            if (t->u == cflat__nfa_epsilon) {
                u32 target = t->v;
                if (!states[target]) {
                    states[target] = 1;
                    epsilons[(*curr_epsilon)++] = target;
                }
            }
        }
    }
}

CflatStringView cflat_nfa_matches(CflatNfa nfa, CflatStringView text) {
    CflatStringView result = {0};
    
    if (nfa.states_count == 0) 
        return result;

    CflatTempArena temp;
    cflat_scratch_arena_scope(temp, 1, &nfa.arena) {

        u8 *curr_states    = cflat_arena_push(temp.arena, sizeof *curr_states  * nfa.states_count);
        u8 *next_states    = cflat_arena_push(temp.arena, sizeof *next_states  * nfa.states_count);
        
        char **curr_starts = cflat_arena_push(temp.arena, sizeof *curr_starts  * nfa.states_count);
        char **next_starts = cflat_arena_push(temp.arena, sizeof *next_starts  * nfa.states_count);
        
        u32 *epsilons = cflat_arena_push(temp.arena, sizeof *epsilons * nfa.states_count);
        usize curr_epsilon = 0;
        
        u32 last_index = (u32)nfa.states_count - 1;

        for (usize start_index = 0; start_index <= text.length; ++start_index) {
            cflat_mem_zero(curr_states, nfa.states_count);
            curr_epsilon = 0;

            curr_states[0] = 1;
            curr_starts[0] = text.data + start_index;
            epsilons[curr_epsilon++] = 0;
            
            cflat__nfa_expand_stack(nfa, curr_states, epsilons, &curr_epsilon);
            for (u32 j = 0; j < nfa.states_count; ++j) {
                if (curr_states[j]) curr_starts[j] = text.data + start_index;
            }

            if (curr_states[last_index]) {
                if (result.data == NULL) {
                    result.data = (char*)text.data + start_index;
                    result.length = 0;
                }
            }

            for (usize i = start_index; i < text.length; ++i) {
                cflat_mem_zero(next_states, nfa.states_count);
                curr_epsilon = 0;
                u8 c = (u8)text.data[i];

                for (u32 s = 0; s < (u32)nfa.states_count; ++s) {
                    if (!curr_states[s]) continue;

                    for (CflatAdjU32 *t = nfa.states[s]; t != NULL; t = t->next) {
                        if (t->u == c) {
                            if (!next_states[t->v]) {
                                next_states[t->v] = 1;
                                next_starts[t->v] = curr_starts[s];
                                epsilons[curr_epsilon++] = t->v;
                            }
                        }
                    }
                }

                if (curr_epsilon == 0) break;

                cflat__nfa_expand_stack(nfa, next_states, epsilons, &curr_epsilon);
                for (u32 j = 0; j < nfa.states_count; ++j) {
                    if (next_states[j]) next_starts[j] = text.data + start_index;
                }
                
                cflat_swap(void*, curr_states, next_states);
                cflat_swap(void*, curr_starts, next_starts);

                if (curr_states[last_index]) {
                    usize curr_states_match_len = (i + 1) - start_index;
                    if (result.data == NULL || curr_states_match_len > result.length) {
                        result.data = (char*)text.data + start_index;
                        result.length = curr_states_match_len;
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

#endif
