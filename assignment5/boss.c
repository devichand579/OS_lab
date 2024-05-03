#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

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

int verify_topological_sort(int *graph, int *T, int n) {
    int pos[n];
    for (int i = 0; i < n; i++) {
        pos[T[i]] = i;
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (graph[i*n+j] == 1 && pos[i] >= pos[j]) {
                fprintf(stderr, "Error: Topological sort is invalid\n");
                return 0;
            }
        }
    }
    return 1;
}

int main() {
    FILE *file = fopen("graph.txt", "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    int n;
    fscanf(file, "%d", &n); // First line contains the value of n

    int key1,key2,key3,key4,key5,key6;
    key1 = ftok("graph.txt", 65);
    key2 = ftok("graph.txt", 66);
    key3 = ftok("graph.txt", 67);
    key4 = ftok("graph.txt", 68);
    key5 = ftok("graph.txt", 69);
    key6 = ftok("graph.txt", 70);

    int shm_id = shmget(key1, n*n*sizeof(int), 0666|IPC_CREAT); // Allocate shared memory for graph
    int *graph = (int*) shmat(shm_id, (void*)0, 0); // Attach to the shared memory for graph

    // Read the adjacency matrix from graph.txt starting from the second line
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (fscanf(file, "%d", &graph[i*n + j]) != 1) {
                perror("Failed to read the matrix from file");
                fclose(file);
                // Properly detach and remove shared memory and semaphores before exiting
                shmdt(graph);
                shmctl(shm_id, IPC_RMID, NULL);
                // Additional cleanup for other resources might be required here
                return 2;
            }
        }
    }
    fclose(file);



    int T_shm_id = shmget(key2, n*sizeof(int), 0666|IPC_CREAT); // Allocate shared memory for T
    int *T = (int*) shmat(T_shm_id, (void*)0, 0); // Attach to the shared memory for T

    int idx_shm_id = shmget(key3, sizeof(int), 0666|IPC_CREAT); // Allocate shared memory for idx
    int *idx = (int*) shmat(idx_shm_id, (void*)0, 0); // Attach to the shared memory for idx
    *idx = 0;

    // Semaphore for mutual exclusion of idx
    int mtc_sem_id = semget(key4, 1, 0666|IPC_CREAT);
    semctl(mtc_sem_id, 0, SETVAL, 1); // Initialize the semaphore to 1

    // Semaphores for sync array
    int sync_sem_id = semget(key5, n, 0666|IPC_CREAT);
    for (int i = 0; i < n; i++) {
        semctl(sync_sem_id, i, SETVAL, 0);
    }

    // Semaphore to notify the boss when a worker has finished
    int nfy_sem_id = semget(key6, 1, 0666|IPC_CREAT);
    semctl(nfy_sem_id, 0, SETVAL, 0); // Initialize the semaphore to 0

    printf("Boss: Setup done...\n");

   int workers_finished = 0;
   while (workers_finished < n) {
    wait_semaphore(nfy_sem_id, 0); // Wait for any worker to signal completion
    workers_finished++;
    }
    printf("Boss: All workers have finished.\n");
    // Check the topological sort
    int flag=0;
    flag=verify_topological_sort(graph, T, n);

    printf("Boss: Topological sort: ");
    if(flag==1){
    for(int i=0;i < n ;i++){
        printf("%d ",T[i]);
    }
    printf("\nBoss: Well done, my team...\n");
    }
    else{
        printf("Topological sort is invalid");
    }

    // Detach from shared memory
    shmdt(graph);
    shmdt(T);
    shmdt(idx);

    // Remove shared memory segments
    shmctl(shm_id, IPC_RMID, NULL);
    shmctl(T_shm_id, IPC_RMID, NULL);
    shmctl(idx_shm_id, IPC_RMID, NULL);

    // Remove semaphores
    semctl(mtc_sem_id, 0, IPC_RMID, NULL);
    semctl(sync_sem_id, 0, IPC_RMID, NULL);
    semctl(nfy_sem_id, 0, IPC_RMID, NULL);

    return 0;
}
