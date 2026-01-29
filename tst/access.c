#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct slice {
    int *data;
    unsigned long long length;
    unsigned long long capacity;
    char undefined_behavior_my_ass[];
};

typedef struct linear_congruential_generator {
    unsigned long long state;
    unsigned long long multiplier; 
    unsigned long long increment;
} LinearCongruentialGenerator;

LinearCongruentialGenerator distinct_sequence_rng(unsigned long long seed, unsigned long long increment) {
    return (LinearCongruentialGenerator) {
        .state = seed,
        .increment = increment | 1,
        .multiplier = (6364136223846793005ULL & ~3ULL) | 1,
    };
}

unsigned long long lcg_rand_next_u64(LinearCongruentialGenerator *lcg, unsigned long long max_exclusive) {
    unsigned long long mask = max_exclusive - 1;
    return lcg->state = (lcg->multiplier * lcg->state + lcg->increment) & mask; 
}

struct slice *contiguous_slice(unsigned long long length) {
    struct slice *slice;
    const unsigned long long size = sizeof(*slice) + length*sizeof(int);
    slice = malloc(size);
    slice->length = length;
    slice->data = (int*)(slice + 1);
    slice->capacity = length;
    memset(slice->data, 0, length * sizeof(int));
    return slice;
}

struct slice stack_metadata(unsigned long long length) {
    struct slice slice;
    slice.length = length;
    slice.capacity = length;
    slice.data = malloc(length * sizeof(int));
    memset(slice.data, 0, length * sizeof(int));
    return slice;
}

void bench_contiguous_slice_seq(struct slice *slice) {
    volatile unsigned long long sum = 0;
    volatile unsigned long long dummy = 0;
    for (unsigned long long i = 0; i < slice->length; ++i) {
        slice->data[i] = i;
        sum += slice->data[i];
        dummy += slice->capacity - slice->length;
    }
}

void bench_stack_metadata_seq(struct slice slice) {
    volatile unsigned long long dummy = 0;
    volatile unsigned long long sum = 0;
    for (unsigned long long i = 0; i < slice.length; ++i) {
        slice.data[i] = i;
        sum += slice.data[i];
        dummy += slice.capacity - slice.length;
    }
}

void bench_contiguous_slice_rand(struct slice *slice) {
    LinearCongruentialGenerator lcg = distinct_sequence_rng((unsigned long long)(uintptr_t)slice->data, (unsigned long long)time(NULL));
    volatile unsigned long long sum = 0;
    volatile unsigned long long dummy = 0;
    for (unsigned long long i = 0; i < slice->length; ++i) {
        slice->data[i] = lcg_rand_next_u64(&lcg, slice->length);
        sum += slice->data[i];
        dummy += slice->capacity - slice->length;
    }
}

void bench_stack_metadata_rand(struct slice slice) {
    LinearCongruentialGenerator lcg = distinct_sequence_rng((unsigned long long)(uintptr_t)slice.data, (unsigned long long)time(NULL));
    volatile unsigned long long sum = 0;
    volatile unsigned long long dummy = 0;
    for (unsigned long long i = 0; i < slice.length; ++i) {
        slice.data[i] = lcg_rand_next_u64(&lcg, slice.length);
        sum += slice.data[i];
        dummy += slice.capacity - slice.length;
    }
}

void print_random_numbers(unsigned long long length) {
    LinearCongruentialGenerator lcg = distinct_sequence_rng(0, (unsigned long long)time(NULL));
    for (unsigned long long i = 0; i < length; ++i) {
        unsigned long long rand = lcg_rand_next_u64(&lcg, length);
        printf("%llu\n", rand);
    }
}

int main(int argc, char **argv) {

    srand(0);
    unsigned long rand_size = rand() % (INT32_MAX - INT16_MAX) + INT16_MAX;

    struct slice *c_slice = contiguous_slice(INT32_MAX);
    struct slice s_slice = stack_metadata(INT32_MAX);
    
    if (argc < 2) {
        printf("Usage: access <test_id>\n");
        return 1;
    }

    char *program_name = argv[0];
    int test_id = atoi(argv[1]);
    if (test_id == 0) {
        bench_contiguous_slice_seq(c_slice);
    } else if (test_id == 1) {
        bench_stack_metadata_seq(s_slice);
    }
    else if (test_id == 2) {
        bench_contiguous_slice_rand(c_slice);
    } else if (test_id == 3) {
        bench_stack_metadata_rand(s_slice);
    }
    else {
        printf("Invalid test id\n");
        return 1;
    }
    return 0;
}