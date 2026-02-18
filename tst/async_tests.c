#include "pthread.h"
#include <stdatomic.h>
#define CFLAT_IMPLEMENTATION
#include "../src/CflatAsyncStack.h"
#include <stdio.h>

_Atomic int count;

void* hello(void* arg) {
    (void)arg;
    printf("Hello, %d\n", atomic_fetch_add(&count, 1));
    return (void*)(uptr)pthread_self();
}

int main(void) {

    Arena *a = arena_new();

    pthread_t self = pthread_self();
    printf("Main Trhead Id %llu\n", self);

    CflatAsyncQueue *stack = cflat_async_work_queue_new(a, 1024, 16);

    CflatAsyncHandle h0 = cflat_async_enqueue(stack, hello, NULL);
    CflatAsyncHandle h1 = cflat_async_enqueue(stack, hello, NULL);
    CflatAsyncHandle h2 = cflat_async_enqueue(stack, hello, NULL);
    CflatAsyncHandle h3 = cflat_async_enqueue(stack, hello, NULL);

    cflat_async_queue_join(stack);

    pthread_t th0 = (pthread_t)cflat_async_queue_result(stack, h0);
    printf("\nTask 0 Trhead Id %llu\n", th0);
    printf("Task 0 Handle %llu\n", h0.index);
    
    pthread_t th1 = (pthread_t)cflat_async_queue_result(stack, h1);
    printf("\nTask 1 Trhead Id %llu\n", th1);
    printf("Task 1 Handle %llu\n", h1.index);
    
    pthread_t th2 = (pthread_t)cflat_async_queue_result(stack, h2);
    printf("\nTask 2 Trhead Id %llu\n", th2);
    printf("Task 2 Handle %llu\n", h2.index);
    
    pthread_t th3 = (pthread_t)cflat_async_queue_result(stack, h3);
    printf("\nTask 3 Trhead Id %llu\n", th3);
    printf("Task 3 Handle %llu\n", h3.index);

    cflat_async_queue_shutdown(stack);
    fflush(stdout); 
}