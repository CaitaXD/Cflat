#include <stdint.h>
#include <stdio.h>
#if 0 && BASH
#!usr/bin/bash
clang dfa_tests.c -g -fsanitize=address -o dfa_tests.script
./dfa_tests.script
rm ./dfa_tests.script
exit 0
#endif

#include <limits.h>
#include "unitest.h"

#define CFLAT_IMPLEMENTATION
#include "../src/CflatArena.h"
#include "../src/CflatString.h"
#include "../src/CflatRegex.h"

typedef void testfn(void);

void sv_find_index_should_work(void) {
    ASSERT_EQUAL(sv_find_index(sv_from_cstr("BarFooFoo"), "Foo"), 3L, "%ld");
    ASSERT_EQUAL(sv_find_index(sv_from_cstr("BarFooFoo"), "FooFoo"), 3L, "%ld");
    ASSERT_EQUAL(sv_find_index(sv_from_cstr("BarFooFoo"), "BarFooFoo"), 0L, "%ld");
    ASSERT_EQUAL(sv_find_index(sv_from_cstr("BarFooFoo"), "BarFoo"), 0L, "%ld");
}

void sv_find_last_index_should_work(void) {
    ASSERT_EQUAL(sv_find_last_index(sv_from_cstr("BarFooFoo"), "Foo"), 6L, "%ld");
    ASSERT_EQUAL(sv_find_last_index(sv_from_cstr("BarFooFoo"), "FooFoo"), 3L, "%ld");
    ASSERT_EQUAL(sv_find_last_index(sv_from_cstr("BarFooFoo"), "BarFooFoo"), 0L, "%ld");
    ASSERT_EQUAL(sv_find_last_index(sv_from_cstr("BarFooFoo"), "BarFoo"), 0L, "%ld");
}

void nfa_literal_match_test(void) {
    CflatArena *arena = cflat_arena_new(); 
    CflatNfa nfa = cflat_nfa_new(arena);  
  
    cflat_nfa_match_opt(&nfa, sv_from_cstr("hello"), CFLAT_MATCH_ONE); 
  
    CflatStringView m1 = cflat_nfa_matches(nfa, sv_from_cstr("hello")); 
    ASSERT_NOT_NULL(m1.data);
    ASSERT_EQUAL(m1.length, 5L, "%lu");
 
    CflatStringView m2 = cflat_nfa_matches(nfa, sv_from_cstr("hell")); 
    ASSERT_NULL(m2.data);
    ASSERT_EQUAL(m2.length, 0L, "%lu");

    CflatStringView m3 = cflat_nfa_matches(nfa, sv_from_cstr("hello world")); 
    ASSERT_NOT_NULL(m3.data);
    ASSERT_EQUAL(m3.length, 5L, "%lu");

    cflat_arena_delete(arena);   
}

void nfa_case_match_test(void) {
    CflatArena *arena = cflat_arena_new(); 
    CflatNfa nfa = cflat_nfa_new(arena);  
  
    cflat_nfa_match_opt(&nfa, sv_from_cstr("hello"), CFLAT_MATCH_ONE | CFLAT_CASE_SENSTIVE); 
  
    CflatStringView m1 = cflat_nfa_matches(nfa, sv_from_cstr("hEllo")); 
    ASSERT_NULL(m1.data);
    ASSERT_EQUAL(m1.length, 0L, "%lu");

    nfa = cflat_nfa_new(arena);
    cflat_nfa_match_opt(&nfa, sv_from_cstr("hello"), CFLAT_MATCH_ONE | CFLAT_CASE_INSENSTIVE); 
    
    CflatStringView m2 = cflat_nfa_matches(nfa, sv_from_cstr("hEllo")); 
    ASSERT_NOT_NULL(m2.data);
    ASSERT_EQUAL(m2.length, 5L, "%lu");

    cflat_arena_delete(arena);   
}

void nfa_match_next_test(void) { 
    CflatArena *arena = cflat_arena_new();
    CflatNfa nfa = cflat_nfa_new(arena); 
     
    cflat_nfa_match_opt(&nfa, sv_from_cstr("hello"), CFLAT_MATCH_ONE);
       
    CflatStringView text = sv_from_cstr("hello world hello world hello");
       
    CflatStringView match = {0}; 
    bool next = cflat_nfa_match_next(nfa, &match, text);
    int count = 1;  
    ASSERT_TRUE(next);  
    do {   
        ASSERT_TRUE(match.data != NULL && match.length == 5);
    } while((next = cflat_nfa_match_next(nfa, &match, text)) && count++);
       
    ASSERT_EQUAL(count, 3, "%d");
  
    cflat_arena_delete(arena);  
}

void nfa_match_zero_or_more(void) {  
    CflatArena *arena = cflat_arena_new(); 
    CflatNfa nfa = cflat_nfa_new(arena);  
  
    cflat_nfa_match_opt(&nfa, sv_from_cstr("AB"), CFLAT_MATCH_ZERO_OR_MORE); 
  
    CflatStringView m1 = cflat_nfa_matches(nfa, sv_from_cstr("ABAB")); 
    ASSERT_NOT_NULL(m1.data);
    ASSERT_EQUAL(m1.length, 4L, "%lu");

    CflatStringView m2 = cflat_nfa_matches(nfa, sv_from_cstr("")); 
    ASSERT_NOT_NULL(m2.data);
    ASSERT_EQUAL(m2.length, 0L, "%lu");
   
    CflatStringView m3 = cflat_nfa_matches(nfa, sv_from_cstr("BBAA")); 
    ASSERT_NOT_NULL(m3.data);
    ASSERT_EQUAL(m3.length, 0L, "%lu");
   
    cflat_arena_delete(arena);   
}

void nfa_match_one_or_more(void) {  
    CflatArena *arena = cflat_arena_new(); 
    CflatNfa nfa = cflat_nfa_new(arena);  
  
    cflat_nfa_match_opt(&nfa, sv_from_cstr("AB"), CFLAT_MATCH_ONE_OR_MORE); 
  
    CflatStringView m1 = cflat_nfa_matches(nfa, sv_from_cstr("ABAB")); 
    ASSERT_NOT_NULL(m1.data);
    ASSERT_EQUAL(m1.length, 4L, "%lu");

    CflatStringView m2 = cflat_nfa_matches(nfa, sv_from_cstr("")); 
    ASSERT_NULL(m2.data);
    ASSERT_EQUAL(m2.length, 0L, "%lu");
   
    CflatStringView m3 = cflat_nfa_matches(nfa, sv_from_cstr("BBAA")); 
    ASSERT_NULL(m3.data);
    ASSERT_EQUAL(m3.length, 0L, "%lu");
   
    cflat_arena_delete(arena);   
}

void nfa_match_concat(void) {
    CflatArena *arena = cflat_arena_new(); 
    CflatNfa nfa = cflat_nfa_new(arena);  
  
    cflat_nfa_match_opt(&nfa, sv_from_cstr("A"), CFLAT_MATCH_ONE); 
    cflat_nfa_match_opt(&nfa, sv_from_cstr("B"), CFLAT_MATCH_ONE); 
  
    CflatStringView m1 = cflat_nfa_matches(nfa, sv_from_cstr("AB")); 
    ASSERT_NOT_NULL(m1.data);
    ASSERT_EQUAL(m1.length, 2L, "%lu");
 
    cflat_arena_delete(arena);
}

void nfa_match_concat_one_or_more(void) {
    CflatArena *arena = cflat_arena_new(); 
    CflatNfa nfa = cflat_nfa_new(arena);  
  
    cflat_nfa_match_opt(&nfa, sv_from_cstr("A"), CFLAT_MATCH_ONE_OR_MORE); 
    cflat_nfa_match_opt(&nfa, sv_from_cstr("B"), CFLAT_MATCH_ONE); 
  
    CflatStringView m1 = cflat_nfa_matches(nfa, sv_from_cstr("AAB")); 
    ASSERT_NOT_NULL(m1.data);
    ASSERT_EQUAL(m1.length, 3L, "%lu");
 
    CflatStringView m2 = cflat_nfa_matches(nfa, sv_from_cstr("B")); 
    ASSERT_NULL(m2.data);
    ASSERT_EQUAL(m2.length, 0L, "%lu");

    CflatStringView m3 = cflat_nfa_matches(nfa, sv_from_cstr("AB")); 
    ASSERT_NOT_NULL(m3.data);
    ASSERT_EQUAL(m3.length, 2L, "%lu");

    cflat_arena_delete(arena);
}

void nfa_match_concat_zero_or_more(void) {
    CflatArena *arena = cflat_arena_new(); 
    CflatNfa nfa = cflat_nfa_new(arena);  
  
    cflat_nfa_match_opt(&nfa, sv_from_cstr("A"), CFLAT_MATCH_ZERO_OR_MORE); 
    cflat_nfa_match_opt(&nfa, sv_from_cstr("B"), CFLAT_MATCH_ONE); 
  
    CflatStringView m1 = cflat_nfa_matches(nfa, sv_from_cstr("AAB")); 
    ASSERT_NOT_NULL(m1.data);
    ASSERT_EQUAL(m1.length, 3L, "%lu");
 
    CflatStringView m2 = cflat_nfa_matches(nfa, sv_from_cstr("B")); 
    ASSERT_NOT_NULL(m2.data);
    ASSERT_EQUAL(m2.length, 1L, "%lu");

    CflatStringView m3 = cflat_nfa_matches(nfa, sv_from_cstr("AB")); 
    ASSERT_NOT_NULL(m3.data);
    ASSERT_EQUAL(m3.length, 2L, "%lu");

    cflat_arena_delete(arena);
}

void nfa_match_zero_or_one(void) {
    CflatArena *arena = cflat_arena_new();
    CflatNfa nfa = cflat_nfa_new(arena);
    
    // Pattern (AB)?
    cflat_nfa_match_opt(&nfa, sv_from_cstr("AB"), CFLAT_MATCH_ZERO_OR_ONE);

    CflatStringView m1 = cflat_nfa_matches(nfa, sv_from_cstr("AB"));
    ASSERT_NOT_NULL(m1.data);
    ASSERT_EQUAL(m1.length, 2L, "%lu");

    CflatStringView m2 = cflat_nfa_matches(nfa, sv_from_cstr(""));
    ASSERT_NOT_NULL(m2.data);
    ASSERT_EQUAL(m2.length, 0L, "%lu");

    CflatStringView m3 = cflat_nfa_matches(nfa, sv_from_cstr("ABAB"));
    ASSERT_NOT_NULL(m3.data);
    ASSERT_EQUAL(m3.length, 2L, "%lu");

    cflat_arena_delete(arena);
}

void nfa_match_concat_zero_or_one(void) {
    CflatArena *arena = cflat_arena_new();
    CflatNfa nfa = cflat_nfa_new(arena);

    // Pattern: AB
    cflat_nfa_match_opt(&nfa, sv_from_cstr("A"), CFLAT_MATCH_ZERO_OR_ONE);
    cflat_nfa_match_opt(&nfa, sv_from_cstr("B"), CFLAT_MATCH_ONE);

    CflatStringView m1 = cflat_nfa_matches(nfa, sv_from_cstr("AB"));
    ASSERT_NOT_NULL(m1.data);
    ASSERT_EQUAL(m1.length, 2L, "%lu");

    CflatStringView m2 = cflat_nfa_matches(nfa, sv_from_cstr("B"));
    ASSERT_NOT_NULL(m2.data);
    ASSERT_EQUAL(m2.length, 1L, "%lu");

    CflatStringView m3 = cflat_nfa_matches(nfa, sv_from_cstr("AAB"));
    ASSERT_NOT_NULL(m3.data);
    ASSERT_EQUAL(m3.length, 2L, "%lu");

    cflat_arena_delete(arena);
}

void nfa_match_alternation_basic(void) {
    CflatArena *arena = cflat_arena_new();
    CflatNfa nfa = cflat_nfa_new(arena);

    // Pattern: A|B
    cflat_nfa_match_opt(&nfa, sv_from_cstr("A"), CFLAT_MATCH_ONE);
    cflat_nfa_or_opt(&nfa, sv_from_cstr("B"), CFLAT_MATCH_ONE);

    ASSERT_EQUAL(cflat_nfa_matches(nfa, sv_from_cstr("A")).length, 1L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa, sv_from_cstr("B")).length, 1L, "%lu");
    ASSERT_NULL(cflat_nfa_matches(nfa, sv_from_cstr("C")).data);

    cflat_arena_delete(arena);
}

void nfa_match_alternation_quantified_lhs(void) {
    CflatArena *arena = cflat_arena_new();
    
    // 1. A*|B
    CflatNfa nfa1 = cflat_nfa_new(arena);
    cflat_nfa_match_opt(&nfa1, sv_from_cstr("A"), CFLAT_MATCH_ZERO_OR_MORE);
    cflat_nfa_or_opt(&nfa1, sv_from_cstr("B"), CFLAT_MATCH_ONE);
    
    ASSERT_EQUAL(cflat_nfa_matches(nfa1, sv_from_cstr("AAA")).length, 3L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa1, sv_from_cstr("B")).length, 1L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa1, sv_from_cstr("")).length, 0L, "%lu"); // A* matches empty

    // 2. A+|B
    CflatNfa nfa2 = cflat_nfa_new(arena);
    cflat_nfa_match_opt(&nfa2, sv_from_cstr("A"), CFLAT_MATCH_ONE_OR_MORE);
    cflat_nfa_or_opt(&nfa2, sv_from_cstr("B"), CFLAT_MATCH_ONE);

    ASSERT_EQUAL(cflat_nfa_matches(nfa2, sv_from_cstr("AAA")).length, 3L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa2, sv_from_cstr("B")).length, 1L, "%lu");
    ASSERT_NULL(cflat_nfa_matches(nfa2, sv_from_cstr("")).data); // A+ requires at least one

    // 3. A?|B
    CflatNfa nfa3 = cflat_nfa_new(arena);
    cflat_nfa_match_opt(&nfa3, sv_from_cstr("A"), CFLAT_MATCH_ZERO_OR_ONE);
    cflat_nfa_or_opt(&nfa3, sv_from_cstr("B"), CFLAT_MATCH_ONE);

    ASSERT_EQUAL(cflat_nfa_matches(nfa3, sv_from_cstr("A")).length, 1L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa3, sv_from_cstr("B")).length, 1L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa3, sv_from_cstr("")).length, 0L, "%lu");

    cflat_arena_delete(arena);
}

void nfa_match_alternation_quantified_rhs(void) {
    CflatArena *arena = cflat_arena_new();

    // 1. A|B*
    CflatNfa nfa1 = cflat_nfa_new(arena);
    cflat_nfa_match_opt(&nfa1, sv_from_cstr("A"), CFLAT_MATCH_ONE);
    cflat_nfa_or_opt(&nfa1, sv_from_cstr("B"), CFLAT_MATCH_ZERO_OR_MORE);

    ASSERT_EQUAL(cflat_nfa_matches(nfa1, sv_from_cstr("A")).length, 1L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa1, sv_from_cstr("BBB")).length, 3L, "%lu");

    // 2. A|B+
    CflatNfa nfa2 = cflat_nfa_new(arena);
    cflat_nfa_match_opt(&nfa2, sv_from_cstr("A"), CFLAT_MATCH_ONE);
    cflat_nfa_or_opt(&nfa2, sv_from_cstr("B"), CFLAT_MATCH_ONE_OR_MORE);

    ASSERT_EQUAL(cflat_nfa_matches(nfa2, sv_from_cstr("A")).length, 1L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa2, sv_from_cstr("BBB")).length, 3L, "%lu");

    // 3. A|B?
    CflatNfa nfa3 = cflat_nfa_new(arena);
    cflat_nfa_match_opt(&nfa3, sv_from_cstr("A"), CFLAT_MATCH_ONE);
    cflat_nfa_or_opt(&nfa3, sv_from_cstr("B"), CFLAT_MATCH_ZERO_OR_ONE);

    ASSERT_EQUAL(cflat_nfa_matches(nfa3, sv_from_cstr("A")).length, 1L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa3, sv_from_cstr("B")).length, 1L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa3, sv_from_cstr("")).length, 0L, "%lu");

    cflat_arena_delete(arena);
}

void test_nfa_groups(void) {
    CflatArena *arena = cflat_arena_new();
    CflatNfa nfa;

    // --- TEST 1: Basic Grouping (AB)+ ---
    nfa = cflat_nfa_new(arena);
    cflat_nfa_begin_group(&nfa);
    cflat_nfa_match_opt(&nfa, cflat_sv_from_cstr("AB"), CFLAT_MATCH_ONE);
    cflat_nfa_end_group_opt(&nfa, CFLAT_MATCH_ONE_OR_MORE);

    ASSERT_EQUAL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("AB")).length, 2L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("ABAB")).length, 4L, "%lu");
    ASSERT_NULL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("A")).data);

    // --- TEST 2: Nested Groups ((A)B)* ---
    nfa = cflat_nfa_new(arena);
    cflat_nfa_begin_group(&nfa);
        cflat_nfa_begin_group(&nfa);
        cflat_nfa_match_opt(&nfa, cflat_sv_from_cstr("A"), CFLAT_MATCH_ONE);
        cflat_nfa_end_group_opt(&nfa, CFLAT_MATCH_ONE);
    cflat_nfa_match_opt(&nfa, cflat_sv_from_cstr("B"), CFLAT_MATCH_ONE);
    cflat_nfa_end_group_opt(&nfa, CFLAT_MATCH_ZERO_OR_MORE);

    ASSERT_EQUAL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("")).length, 0L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("AB")).length, 2L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("B")).length, 1L, "%lu");

    // --- TEST 3: Group with Alternation (A|B)+ ---
    nfa = cflat_nfa_new(arena);
    cflat_nfa_begin_group(&nfa);
    cflat_nfa_match_opt(&nfa, cflat_sv_from_cstr("A"), CFLAT_MATCH_ONE);
    cflat_nfa_or_opt(&nfa, cflat_sv_from_cstr("B"), CFLAT_MATCH_ONE);
    cflat_nfa_end_group_opt(&nfa, CFLAT_MATCH_ONE_OR_MORE);

    ASSERT_EQUAL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("A")).length, 1L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("B")).length, 1L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("ABA")).length, 3L, "%lu");
    ASSERT_NULL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("C")).data);

    // --- TEST 4: Optional Group (ABC)? ---
    nfa = cflat_nfa_new(arena);
    cflat_nfa_begin_group(&nfa);
    cflat_nfa_match_opt(&nfa, cflat_sv_from_cstr("ABC"), CFLAT_MATCH_ONE);
    cflat_nfa_end_group_opt(&nfa, CFLAT_MATCH_ZERO_OR_ONE);

    ASSERT_EQUAL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("")).length, 0L, "%lu");
    ASSERT_EQUAL(cflat_nfa_matches(nfa, cflat_sv_from_cstr("ABC")).length, 3L, "%lu");
    CflatStringView m = cflat_nfa_matches(nfa, cflat_sv_from_cstr("D"));
    ASSERT_NOT_NULL(m.data);
    ASSERT_EQUAL(m.length, 0L, "%lu");

    cflat_arena_delete(arena);
}

int main() {
    
    sv_find_index_should_work();
    sv_find_last_index_should_work();

    nfa_literal_match_test();
    nfa_case_match_test();
    nfa_match_next_test();
    nfa_match_one_or_more();
    nfa_match_zero_or_more();
    nfa_match_concat_zero_or_one();

    nfa_match_concat();
    nfa_match_concat_one_or_more();
    nfa_match_concat_zero_or_more();
    nfa_match_concat_zero_or_one();

    nfa_match_alternation_basic();
    nfa_match_alternation_quantified_lhs();
    nfa_match_alternation_quantified_rhs();

    test_nfa_groups();

    printf("All Tests Passed\n");
    return 0;
}
