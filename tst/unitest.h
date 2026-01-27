#ifndef CFLAT_UNITEST_H
#define CFLAT_UNITEST_H

#define ENCLOSE_EXP(e) "{ "#e" }"

#define ASSERT_BINARY_PREDICATE(a, e, OP, ARG_FMT) do {                                             \
    if (((a) OP (e)) == false) {                                                                    \
        fprintf(stderr, "%s [%s:%d]\n", __func__, __FILE__, __LINE__);                              \
        fputs("Assertion Failed ("#a " "#OP" " #e")", stderr);                                      \
        fprintf(stderr, "Expected: " ARG_FMT, e);                                                   \
        fputs(ENCLOSE_EXP(e), stderr);                                                              \
        fprintf(stderr, "Actual: " ARG_FMT, a);                                                     \
        fputs(ENCLOSE_EXP(a), stderr);                                                              \
        cflat_trap();                                                                               \
        exit(1);                                                                                    \
    }                                                                                               \
} while (0)

#define ASSERT_TRUE(e) ASSERT_BINARY_PREDICATE(e, true, ==, "%d")
#define ASSERT_FALSE(e) ASSERT_BINARY_PREDICATE(e, false, ==, "%d")

#define ASSERT_EQUAL(a, e, ARG_FMT) ASSERT_BINARY_PREDICATE(a, e, ==, ARG_FMT)
#define ASSERT_NOT_EQUAL(a,e, ARG_FMT) ASSERT_BINARY_PREDICATE(a, e, !=, ARG_FMT)
#define ASSERT_LESS_THAN(a, e, ARG_FMT) ASSERT_BINARY_PREDICATE(a, e, <, ARG_FMT)
#define ASSERT_GREATER_THAN(a, e, ARG_FMT) ASSERT_BINARY_PREDICATE(a, e, >, ARG_FMT)
#define ASSERT_GREATER_OR_EQUAL(a, e, ARG_FMT) ASSERT_BINARY_PREDICATE(a, e, >=, ARG_FMT)
#define ASSERT_LESS_OR_EQUAL(a, e, ARG_FMT) ASSERT_BINARY_PREDICATE(a, e, <=, ARG_FMT)

#define ASSERT_NULL(a) ASSERT_EQUAL((void*)a, NULL, "%p");
#define ASSERT_NOT_NULL(a) ASSERT_NOT_EQUAL((void*)a, NULL, "%p");

#endif //CFLAT_UNITEST_H
