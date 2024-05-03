// foothread.h

#ifndef FOO_THREAD_H
#define FOO_THREAD_H
#define _GNU_SOURCE
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>

// Maximum number of threads allowed
#define FOOTHREAD_THREADS_MAX 100

// Default stack size for threads (2MB)
#define FOOTHREAD_DEFAULT_STACK_SIZE (2 * 1024 * 1024)


// Join type constants
#define FOOTHREAD_JOINABLE 0
#define FOOTHREAD_DETACHED 1

#define FOOTHREAD_ATTR_INITIALZER {FOOTHREAD_DETACHED,FOOTHREAD_DEFAULT_STACK_SIZE}


// Thread function type
typedef void *(*ThreadFunc)(void *);

// Thread structure
typedef struct {
    pid_t leader_pid;             // PID of the leader thread
    pid_t tid;                    // TID of the follower thread
    void *(*start_routine)(void *);
    void *arg;
    int detach_state;             // Detached or joinable state
    int joined;                   // Flag to track if thread has been joined
} foothread_t;

// Thread attribute structure
typedef struct {
    int join_type;                // Join type (joinable or detached)
    int stack_size;               // Stack size
} foothread_attr_t;

// Mutex structure with an additional flag to track the locked state
typedef struct {
    sem_t mutex;
    int locked; // Flag to track the locked state
} foothread_mutex_t;

// Barrier structure
typedef struct {
    sem_t barrier_sem1;
    sem_t barrier_sem2;
    int count;
    int num_threads;
} foothread_barrier_t;

// Function prototypes
int foothread_create(foothread_t *thread, foothread_attr_t *attr, ThreadFunc func, void *arg);
void foothread_attr_setjointype ( foothread_attr_t * , int ) ;
void foothread_attr_setstacksize ( foothread_attr_t * , int ) ;
void foothread_exit();
void foothread_mutex_init(foothread_mutex_t *mutex);
void foothread_mutex_lock(foothread_mutex_t *mutex);
void foothread_mutex_unlock(foothread_mutex_t *mutex);
void foothread_mutex_destroy(foothread_mutex_t *mutex);
void foothread_barrier_init(foothread_barrier_t *barrier, int num_threads);
void foothread_barrier_wait(foothread_barrier_t *barrier);
void foothread_barrier_destroy(foothread_barrier_t *barrier);

#endif
