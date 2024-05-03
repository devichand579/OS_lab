#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>

struct sembuf sb_wait = {0, -1, 1};  // Global semaphore operation for wait
struct sembuf sb_signal = {0, 1, 1}; // Global semaphore operation for signal

void wait_semaphore(int sem_id, int sem_num) {
    sb_wait.sem_num = sem_num; // Set the semaphore number dynamically
    semop(sem_id, &sb_wait, 1);
}

void signal_semaphore(int sem_id, int sem_num) {
    sb_signal.sem_num = sem_num; // Set the semaphore number dynamically
    semop(sem_id, &sb_signal, 1);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <n> <ID>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int id = atoi(argv[2]);

    int key1,key2,key3,key4,key5,key6;
    key1 = ftok("graph.txt", 65);
    key2 = ftok("graph.txt", 66);
    key3 = ftok("graph.txt", 67);
    key4 = ftok("graph.txt", 68);
    key5 = ftok("graph.txt", 69);
    key6 = ftok("graph.txt", 70);

    // Attach to the shared memory segments for graph, T, and idx
    int graph_shm_id = shmget(key1, n*n*sizeof(int), 0666);
    int *graph = (int *)shmat(graph_shm_id, NULL, 0);

    int T_shm_id = shmget(key2, n*sizeof(int), 0666);
    int *T = (int *)shmat(T_shm_id, NULL, 0);

    int idx_shm_id = shmget(key3, sizeof(int), 0666);
    int *idx = (int*) shmat(idx_shm_id, (void*)0, 0);

    // Attach to the semaphores
    int mtx_sem_id = semget(key4, 1, 0666);
    int sync_sem_id = semget(key5, n, 0666);
    int nfy_sem_id = semget(key6, 1, 0666);


    // Wait for all prerequisites to be completed
    for (int i = 0; i < n; i++) {
        if (graph[i * n + id] == 1) { // Corrected check: Wait for tasks that this task depends on
            wait_semaphore(sync_sem_id, i);
        }
    }

    // Enter critical section to update T and idx
    wait_semaphore(mtx_sem_id, 0);
    T[*idx] = id;
    *idx = *idx + 1;

    // Corrected signaling: Signal all workers that depend on this task
    signal_semaphore(mtx_sem_id, 0);
    for (int i = 0; i < n; i++) {
        if (graph[id * n + i] == 1) { // Corrected check: Signal tasks that depend on this one
            signal_semaphore(sync_sem_id, id);
        }
    }

    // Signal the boss that this worker has finished
    signal_semaphore(nfy_sem_id, 0);

    // Detach from shared memory segments
    shmdt(graph);
    shmdt(T);
    shmdt(idx);

    return 0;
}
