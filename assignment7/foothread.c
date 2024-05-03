// foothread.c

#include "foothread.h"








// Function to initialize a mutex
void foothread_mutex_init(foothread_mutex_t *mutex) {
    sem_init(&(mutex->mutex), 1, 1);  // Initialize semaphore as a mutex
    mutex->locked = 0; // Initialize the locked state to unlocked
}

// Function to lock a mutex
void foothread_mutex_lock(foothread_mutex_t *mutex) {
    sem_wait(&(mutex->mutex));  // Wait until semaphore value becomes positive
    mutex->locked = 1; // Update the locked state
}

// Function to unlock a mutex
void foothread_mutex_unlock(foothread_mutex_t *mutex) {
    if (mutex->locked) { // Check if the mutex is locked
        sem_post(&(mutex->mutex));  // Increment semaphore value
        mutex->locked = 0; // Update the locked state
    } else {
        // Error: Attempting to unlock an unlocked mutex
        // Handle the error appropriately, e.g., print an error message or raise an exception
        fprintf(stderr, "Error: Attempting to unlock an unlocked mutex\n");
        // Handle the error as needed
        exit(EXIT_FAILURE); // For demonstration, exit the program on error
    }
}

// Function to destroy a mutex
void foothread_mutex_destroy(foothread_mutex_t *mutex) {
    sem_destroy(&(mutex->mutex));  // Destroy semaphore
}

// Function to initialize a barrier
void foothread_barrier_init(foothread_barrier_t *barrier, int num_threads) {
    if (num_threads <= 0) {
        perror("Number of threads must be positive");
        exit(EXIT_FAILURE);
    }
    barrier->count = 0;
    barrier->num_threads = num_threads;
    sem_init(&(barrier->barrier_sem1), 0, 0);  // Initialize barrier semaphore 1
    sem_init(&(barrier->barrier_sem2), 0, 1);  // Initialize barrier semaphore 2
}

// Function to wait at a barrier
void foothread_barrier_wait(foothread_barrier_t *barrier) {
    sem_wait(&(barrier->barrier_sem2));  // Ensure only one thread can access count at a time
    barrier->count++;
    if (barrier->count == barrier->num_threads) {
        // Last thread to arrive at the barrier
        sem_post(&(barrier->barrier_sem1));  // Release waiting threads
        sem_wait(&(barrier->barrier_sem2));  // Block until all threads leave
        barrier->count = 0;  // Reset count for next iteration
    } else {
        sem_post(&(barrier->barrier_sem2));  // Release count access
        sem_wait(&(barrier->barrier_sem1));  // Block until all threads arrive
    }
    sem_post(&(barrier->barrier_sem2));  // Release waiting threads
}

// Function to destroy a barrier
void foothread_barrier_destroy(foothread_barrier_t *barrier) {
    sem_destroy(&(barrier->barrier_sem1));
    sem_destroy(&(barrier->barrier_sem2));
}

// Function to set join type attribute
void foothread_attr_setjointype(foothread_attr_t *attr, int join_type) {
    attr->join_type = join_type;
}

// Function to set stack size attribute
void foothread_attr_setstacksize(foothread_attr_t *attr, int stack_size) {
    attr->stack_size = stack_size;
}

//function to exit and synchronization of threads
void foothread_exit() {


}


// Function to create a foothread
int foothread_create(foothread_t *thread, foothread_attr_t *attr, ThreadFunc func, void *arg) {
    // Initialize thread attributes
    thread->detach_state = attr->join_type;
    

    // Set thread function and argument
    thread->start_routine = func;
    thread->arg = arg;

    // Clone the thread
    int clone_flags = CLONE_VM | CLONE_SIGHAND | CLONE_THREAD;
    // Allocate stack for the child thread
    void *stack = malloc(attr->stack_size);
    if (stack == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    thread->tid = clone(thread->start_routine, (char *)stack + attr->stack_size, clone_flags, thread->arg);
        
    if (thread->tid == -1) {
        perror("clone");
        exit(EXIT_FAILURE);
    }

    // Set PID and PPID for the follower thread
    thread->tid = gettid();
    thread->leader_pid = getppid();

    return 0;
}
