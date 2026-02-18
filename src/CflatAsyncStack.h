#include <time.h>
#if !defined(CFLAT_ASYNC_H)
#define CFLAT_ASYNC_H

#include <stdio.h>
#include "CflatArena.h"
#include "stdint.h"
#include <stdatomic.h>
#include <pthread.h>
#include "CflatCore.h"
#include "CflatSlice.h"
#include "CflatAppend.h"
#include <errno.h>

typedef struct cflat_async_handle { 
    usize index; 
} CflatAsyncHandle;

#define cflat_async_handle_is_nil(HANDLE) ((HANDLE).index == 0)
#define cflat_async_handle_nil() (CflatAsyncHandle) { 0 }

typedef void*(CflatTask)(void* arg);

typedef struct cflat_work_queue_item {
    CflatTask *task;
    void *userdata;
    void *result;
    atomic_bool ready;
    atomic_bool completed;
} CflatWorkItem;

typedef struct cflat_semaphore {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    usize counter;
} CflatSemaphore;

typedef struct cflat_async_queue {
    _Atomic usize  head;
    _Atomic usize  tail;
    usize task_count;
    usize thread_count;
    CflatWorkItem *bag;
    pthread_t     *pool;
    CflatSemaphore sem_avaliable;
    atomic_bool running;
} CflatAsyncQueue;

CflatAsyncQueue* cflat_async_work_queue_new(CflatArena *a, usize max_tasks, usize thread_count);
void cflat_async_queue_join (CflatAsyncQueue *sched);
void cflat_async_queue_clear(CflatAsyncQueue *sched);

CflatAsyncHandle cflat_async_enqueue(CflatAsyncQueue *sched, CflatTask *task, void *userdata);
void *cflat_async_queue_result         (CflatAsyncQueue *sched, CflatAsyncHandle handle);
bool cflat_async_queue_is_completed    (CflatAsyncQueue *sched, CflatAsyncHandle handle);

void cflat_sem_init     (CflatSemaphore *sem, usize initial_count);
void cflat_sem_wait     (CflatSemaphore *sem);
void cflat_sem_post     (CflatSemaphore *sem, usize post);
bool cflat_sem_timedwait(CflatSemaphore *sem, struct timespec relative);
bool cflat_sem_trywait  (CflatSemaphore *sem);
void cflat_sem_delete   (CflatSemaphore *sem);

#if defined(CFLAT_IMPLEMENTATION)

CflatAsyncHandle cflat_async_enqueue(CflatAsyncQueue *sched, CflatTask *task, void *userdata) {
    usize index = atomic_fetch_add_explicit(&sched->head, 1, memory_order_relaxed) + 1;
    if (index >= sched->task_count) return cflat_async_handle_nil();
    
    CflatWorkItem *work = &sched->bag[index];
    work->task     = task;
    work->userdata = userdata;
    work->result   = NULL;
    atomic_store_explicit(&work->completed, false, memory_order_release);
    atomic_store_explicit(&work->ready, true, memory_order_relaxed);

    cflat_sem_post(&sched->sem_avaliable, 1);
    return (CflatAsyncHandle) { .index=index };
}

bool cflat_async_queue_is_completed(CflatAsyncQueue *sched, CflatAsyncHandle handle) {
    return atomic_load_explicit(&sched->bag[handle.index].completed, memory_order_acquire);
}

void* cflat_async_queue_result(CflatAsyncQueue *sched, CflatAsyncHandle handle) {
    if (cflat_async_queue_is_completed(sched, handle)) {
        return sched->bag[handle.index].result;
    }
    return NULL;
}

static void cflat__async_queue_call(CflatAsyncQueue *sched) {
    usize index;
    usize current_head;

    while (true) {
        usize current_tail = atomic_load_explicit(&sched->tail, memory_order_acquire);
        current_head = atomic_load_explicit(&sched->head, memory_order_acquire);

        if (current_tail >= current_head) {
            return;
        }

        index = current_tail;
        if (atomic_compare_exchange_weak_explicit(&sched->tail, &index, current_tail + 1, memory_order_release, memory_order_relaxed)) {
            index = current_tail + 1;
            break;
        }
    }

    if (index < sched->task_count) {
        CflatWorkItem *work = &sched->bag[index];
        
        while (!atomic_load_explicit(&work->ready, memory_order_acquire)) {
            sched_yield(); 
        }

        if (work->task) work->result = work->task(work->userdata);
        
        atomic_store_explicit(&work->ready, false, memory_order_release);
        atomic_store_explicit(&work->completed, true, memory_order_release);
    }
}

void* cflat__async_worker(void *arg) {
    CflatAsyncQueue *sched = (CflatAsyncQueue *)arg;
    while (atomic_load_explicit(&sched->running, memory_order_relaxed)) {
        cflat_sem_wait(&sched->sem_avaliable); 
        cflat__async_queue_call(sched);
    }
    return NULL;
}

void cflat_sem_init(CflatSemaphore *sem, usize initial_count) {
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
    sem->counter = initial_count;
}

void cflat_sem_wait(CflatSemaphore *sem) {
    pthread_mutex_lock(&sem->mutex);
    while (sem->counter == 0) {
        pthread_cond_wait(&sem->cond, &sem->mutex);
    }
    sem->counter--;
    pthread_mutex_unlock(&sem->mutex);
}

bool cflat_sem_trywait(CflatSemaphore *sem) {
    if (pthread_mutex_trylock(&sem->mutex) == 0) {
        if (sem->counter == 0) {
            pthread_mutex_unlock(&sem->mutex);
            return false;
        }
        sem->counter--;
        pthread_mutex_unlock(&sem->mutex);
        return true;
    }
    return false;
}

bool cflat_sem_timedwait(CflatSemaphore *sem, struct timespec relative) {
    bool success = false;

    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec  += relative.tv_sec;
    abstime.tv_nsec += relative.tv_nsec;

    pthread_mutex_lock(&sem->mutex);
    while (sem->counter == 0) {
        if (pthread_cond_timedwait(&sem->cond, &sem->mutex, &abstime) == ETIMEDOUT) {
            goto done; 
        }
    }
    sem->counter--;
    success = true;
done:
    pthread_mutex_unlock(&sem->mutex);
    return success;
}

void cflat_sem_post(CflatSemaphore *sem, usize post) {
    pthread_mutex_lock(&sem->mutex);
    sem->counter += post;
        
    for (usize i = 0; i < post; ++i)
        pthread_cond_signal(&sem->cond);

    pthread_mutex_unlock(&sem->mutex);
}

void cflat_sem_reset(CflatSemaphore *sem) {
    pthread_mutex_lock(&sem->mutex);
    sem->counter = 0;
    pthread_mutex_unlock(&sem->mutex);
}

void cflat_sem_delete(CflatSemaphore *sem) {
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->cond);
}

CflatAsyncQueue* cflat_async_work_queue_new(CflatArena *a, usize max_tasks, usize thread_count) {    
    CflatAsyncQueue *sched = cflat_arena_push(a, sizeof(*sched), .clear=false);
    *sched = (CflatAsyncQueue) {
        .running      = true,
        .bag          = cflat_arena_push(a, (max_tasks + 1)*sizeof(*sched->bag),  .clear=true),
        .pool         = cflat_arena_push(a, (thread_count )*sizeof(*sched->pool), .clear=true),
        .task_count    = max_tasks + 1,
        .thread_count = thread_count,
    };

    cflat_sem_init(&sched->sem_avaliable, 0);

    for (usize i = 0; i < thread_count; ++i) {
        pthread_t *th = &sched->pool[i];
        (void)pthread_create(th, NULL, cflat__async_worker, sched);
    }

    return sched;
}

void cflat_async_queue_join(CflatAsyncQueue *sched) {
    while (true) {
        usize tail = atomic_load_explicit(&sched->tail, memory_order_acquire) + 1;
        usize head = atomic_load_explicit(&sched->head, memory_order_acquire) + 1;
        if (tail >= head) break;

        if (cflat_sem_timedwait(&sched->sem_avaliable, (struct timespec){ .tv_nsec = 100*1000 })) {
            cflat__async_queue_call(sched);
        }
    }
}

void cflat_async_queue_clear(CflatAsyncQueue *sched) {
    atomic_store_explicit(&sched->head, 0, memory_order_relaxed);
    atomic_store_explicit(&sched->tail, 0, memory_order_relaxed);
    cflat_sem_reset(&sched->sem_avaliable);
}

void cflat_async_queue_shutdown(CflatAsyncQueue *sched) {
    atomic_store_explicit(&sched->running, false, memory_order_release);
    cflat_sem_post(&sched->sem_avaliable, sched->thread_count); 
    
    for (usize i = 0; i < sched->thread_count; ++i) {
        if (sched->pool[i]) {
            pthread_join(sched->pool[i], NULL); 
        }
    }
    cflat_sem_delete(&sched->sem_avaliable);
}

#endif // CFLAT_IMPLEMENTATION

#endif // CFLAT_ASYNC_H