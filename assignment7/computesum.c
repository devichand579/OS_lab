// computesum.c

#include "foothread.h"
#include <stdio.h>
#include <stdlib.h>

// Global variables
int n;               // Number of nodes in the tree
int *P;              // Parent array of the tree
int *sums;           // Array to store partial sums
foothread_mutex_t *mutexes;  // Array of mutexes for synchronization
foothread_mutex_t *io_mutex;  // Mutex for I/O operations
foothread_barrier_t *barriers; // Barrier for synchronization
int parent[100];
int num_children[100];

void construct_arrays_from_file(const char* file_path) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Read the value of 'n'
    fscanf(file, "%d", &n);

    // Initialize parent and children arrays
    for (int i = 0; i < n; i++) {
        parent[i] = -1;
        num_children[i] = 0;
    }

    // Parse each subsequent line to extract parent-child relationships
    int child, parent_node;
    while (fscanf(file, "%d %d", &child, &parent_node) != EOF) {
        if(child!= parent_node){
        parent[child] = parent_node;
        num_children[parent_node]++;
        }
    }

    fclose(file);
}

// Function to compute sum recursively
void compute_sum(int node) {
    // If node is a leaf node (has no children), read user input
    if (num_children[node] == 0) {
        int input;
        foothread_mutex_lock(io_mutex);
        printf("Lead node %d:: Enter a positive integer: ", node);
        scanf("%d", &input);
        foothread_mutex_unlock(io_mutex);

        // Update parent's sum
        foothread_mutex_lock(&mutexes[parent[node]]);
        sums[node] += input;
        foothread_mutex_unlock(&mutexes[parent[node]]);

        // Signal parent that sum has been updated
        foothread_barrier_wait(&barriers[parent[node]]);
    } else {
        // Wait for children to finish computing sum
        foothread_barrier_wait(&barriers[node]);
        // Update internal node's sum
        int child_sum = 0;
        for (int i = 0; i < n; i++) {
            if (parent[i] == node) {
                child_sum += sums[i];
            }
        }
        printf("Internal node %d gets the partial sum %d from its children\n", node, child_sum);
        sums[node] = child_sum;
    }
    // if node!=root
    // it should signLal to its parent
    if(parent[node]!=-1){
        foothread_barrier_wait(&barriers[parent[node]]);
    }
}


// Thread function
void* thread_func(void *arg) {
    int node = (int *)arg;
    compute_sum(node);
    return NULL;
}

int main() {

    construct_arrays_from_file("tree.txt");


    // Initialize synchronization resources
    barriers = (foothread_barrier_t *)malloc(n * sizeof(foothread_barrier_t));
    for (int i = 0; i < n; i++) {
        if (num_children[i] > 0)
           foothread_barrier_init(&barriers[i], num_children[i]+1);
    }
    mutexes = (foothread_mutex_t *)malloc(n * sizeof(foothread_mutex_t));
    io_mutex = (foothread_mutex_t *)malloc(sizeof(foothread_mutex_t));
    if (mutexes == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n; i++) {
        foothread_mutex_init(&mutexes[i]);
    }
    foothread_mutex_init(io_mutex);

    // Allocate memory for sums array
    sums = (int *)malloc(n * sizeof(int));
    if (sums == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Create threads for each node in the tree
    foothread_t threads[n];
    for (int i = 0; i < n; i++) {
        foothread_attr_t attr = FOOTHREAD_ATTR_INITIALZER;
        foothread_attr_setjointype(&attr, FOOTHREAD_JOINABLE);
        foothread_create(&threads[i], &attr, thread_func, (void *)i);
    }

    sleep(30);
    for (int i = 0; i < n; i++) {
        if(parent[i]==-1){
            printf("Sum of root node %d: %d\n",i, sums[i]);
        }
    }

    // Clean up resources
    
    free(P);
    free(sums);
    free(mutexes);
    for (int i = 0; i < n; i++) {
        foothread_barrier_destroy(&barriers[i]);
    }
    return 0;
}
