#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

#define SHM_SIZE 2


int main() {
    int shm_id;
    int *shm_ptr;
    int i, n, p, item;
    pid_t pid;
    int *consumed_counts;
    int *consumed_sum;

    printf("n=");
    scanf("%d", &n);
    printf("t=");
    scanf("%d", &p);

    consumed_counts = (int *)calloc(n, sizeof(int)); 
    consumed_sum = (int *)calloc(n, sizeof(int)); 

    shm_id = shmget(IPC_PRIVATE, SHM_SIZE * sizeof(int), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        return 1;
    }

    shm_ptr = (int *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (int *)-1) {
        perror("shmat");
        return 1;
    }

    shm_ptr[0] = 0;

    printf("Producer is alive\n");

    for (i = 1; i <=n; i++) {
        pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        } else if (pid == 0) {
            int sum = 0;
            int items=0;
            printf("\t\t\t\tConsumer %d is alive\n", i+1);
            while (1) {
                while(shm_ptr[0] != -1 && shm_ptr[0] != i) ;
            
                if (shm_ptr[0] == i) {
                    sum += shm_ptr[1];
                    items++;
                    shm_ptr[0] = 0;
                    #ifdef VERBOSE
                        printf("\t\t\t\tConsumer %d reads %d\n", i+1, shm_ptr[1]);
                    #endif
                }
                if (shm_ptr[0] == -1) {
                    printf("\t\t\t\tConsumer %d has read %d items: Checksum = %d\n", i+1, items, sum);
                    exit(0);
                   }
            }
        }
    }

    srand(time(NULL));
    for (i = 0; i < p; i++) {
        while(shm_ptr[0] != 0);
        item = rand() % 900 + 100; // Random 3-digit number
        int consumer_num = rand() % n + 1;
        consumed_counts[consumer_num]++;
        consumed_sum[consumer_num] += item;
        shm_ptr[0] = consumer_num;
        #ifdef SLEEP 
            usleep(1);
        #endif
        shm_ptr[1] = item;
        #ifdef VERBOSE
            printf("Producer produces %d for Consumer %d\n", item, consumer_num+1);
        #endif
    }
    while (shm_ptr[0] != 0);
    for (i = 1; i <= n; i++) {
        shm_ptr[0] = -1;
        wait(NULL);
    }

    printf("Producer has produced %d items\n", p);
    for (i = 1; i <= n; i++) {
        printf("%d items for consumer %d : Checksum = %d\n",consumed_counts[i], i+1, consumed_sum[i]);
    }
 
    if (shmdt(shm_ptr) == -1) {
        perror("shmdt");
        return 1;
    }

    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        return 1;
    }

    free(consumed_counts);
    free(consumed_sum);

    return 0;
}
