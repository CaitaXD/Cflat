
#include <stdio.h>
#include <stdlib.h>

struct slice {
    int *data;
    unsigned long long length;
    unsigned long long capacity;
};

int main (void) {
    
    struct slice *slice;
    {
        slice = &(struct slice){ .data = malloc(sizeof(int)*10), .length = 10, .capacity = 10 };
    }

    printf("Length: %llu\n", slice->length);
    printf("Capacity: %llu\n", slice->capacity);

    return 0;
}