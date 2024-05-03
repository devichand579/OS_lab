#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_JOBS 11  // Maximum number of jobs

// Structure for a job
typedef struct {
    pid_t pid;  // Process ID
    pid_t pgid; // Process group ID
    char job_name[10]; // Job name 
    int status; // Status of the job (running, stopped)
} job_t;

job_t jobs[MAX_JOBS]; // Array to store jobs
int job_count = 0;    // Count of current jobs
pid_t current_active_job_pid = -1;
int job_id=0;



// Function to initialize job table
void initialize_jobs() {
    // Set first entry for the manager process
    jobs[0].pid = getpid();      // PID of the manager process
    jobs[0].pgid = getpgid(getpid()); // PGID of the manager process
    strcpy(jobs[0].job_name, "mgr"); // Name of the manager process
    jobs[0].status = 0;          // Status 0 to indicate self

    // Initialize the rest of the jobs array
    for (int i = 1; i < MAX_JOBS; i++) {
        jobs[i].pid = -1;
        jobs[i].pgid = -1;
        strcpy(jobs[i].job_name, "");
        jobs[i].status = -1; // Status 0 to indicate not in use
    }
}

void print_jobs() {
    printf("NO   PID   PGID   STATUS   NAME\n");
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != -1) { // Print only initialized jobs
            if(jobs[i].status == 0){
                printf("%d   %d   %d   SELF   %s\n", i, jobs[i].pid, jobs[i].pgid, jobs[i].job_name);
            }
            else if(jobs[i].status == 1){
                printf("%d   %d   %d   FINISHED   %s\n", i, jobs[i].pid, jobs[i].pgid, jobs[i].job_name);
            }
            else if(jobs[i].status == 2){
                printf("%d   %d   %d   TERMINATED   %s\n", i, jobs[i].pid, jobs[i].pgid, jobs[i].job_name);
            }
            else if(jobs[i].status == 3){
                printf("%d   %d   %d   SUSPENDED   %s\n", i, jobs[i].pid, jobs[i].pgid, jobs[i].job_name);
            }
            else if(jobs[i].status == 4){
                printf("%d   %d   %d   KILLED   %s\n", i, jobs[i].pid, jobs[i].pgid, jobs[i].job_name);
            }
        }
    }
}

void sigint_handler(int sig) {
    if (current_active_job_pid != -1) {
        printf("\n");
        jobs[job_id].status = 2; // Update status
        kill(current_active_job_pid, SIGINT);
        current_active_job_pid=-1;
    }
    else {
        printf("\nmgr> ");
    }
    fflush(stdout);
}

void sigtstp_handler(int sig) {
    if (current_active_job_pid != -1) {
        printf("\n");
        jobs[job_id].status = 3; // Update status
        kill(current_active_job_pid, SIGTSTP);
        current_active_job_pid=-1;
    }
    else {
        printf("\nmgr> ");
    }
    fflush(stdout);
}

// Killing suspended jobs
void clean_up_suspended_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].status == 3) { // 3 indicates SUSPENDED
            kill(jobs[i].pgid, SIGKILL); // Send SIGKILL to the process group of the suspended job
        }
    }
}

// Creating new Job
void start_new_job() {
    // Check for available slot
    if (job_count >= MAX_JOBS - 1) {
        printf("Process table is full. Quitting...\n");
        clean_up_suspended_jobs(); // Ensure handling of any orphaned suspended jobs
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Failed to fork");
        return;
    }
    
    // Generate a random uppercase letter
    srand(time(NULL));
    char letter = 'A' + (rand() % 26);

    if (pid == 0) { // Child process
        char *args[] = {"job", &letter, NULL};

        execvp("./job", args); // Replace the child process with the job program
        perror("exec failed");
        exit(5);
    } else { // Parent process
        setpgid(pid, 0); // Set the child's PGID to its PID
        job_count++;
        job_id = job_count; // Find the next available job ID
        int status;
        jobs[job_id].pid = pid;
        current_active_job_pid = pid;
        jobs[job_id].pgid = getpgid(pid);
        sprintf(jobs[job_id].job_name, "job %c", letter);
        waitpid(pid, &status, WUNTRACED);
        if(status == 0){
            jobs[job_id].status = 1; // Update status  
        }
        current_active_job_pid=-1;
    }
}

void continue_suspended_job() {
    // Check if there are any suspended jobs
    int hasSuspended = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].status == 3) { // 3 indicates SUSPENDED
            hasSuspended = 1;
            break;
        }
    }

    if (!hasSuspended) {
        printf("No suspended jobs.\n");
        return;
    }

    // Prompt the user to select a suspended job
    printf("Suspended jobs: ");
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].status == 3) {
            printf("%d ", i);
        }
    }
    printf("(Pick one): ");

    int selectedJob;
    scanf("%d", &selectedJob);

    // Validate the input
    if (selectedJob < 1 || selectedJob >= MAX_JOBS || jobs[selectedJob].status != 3) {
        printf("Invalid selection.\n");
        return;
    }
    
    // Resume the selected job
    kill(jobs[selectedJob].pid, SIGCONT);

    current_active_job_pid = jobs[selectedJob].pid; 
    printf("Resumed job %d.\n", selectedJob);
    int status;
    waitpid(jobs[selectedJob].pid, &status, WUNTRACED);
    if(status == 0){
            jobs[job_id].status = 1; // Update status  
        }
    current_active_job_pid=-1;
}

void kill_suspended_job() {
    // Check if there are any suspended jobs
    int hasSuspended = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].status == 3) { // 3 indicates SUSPENDED
            hasSuspended = 1;
            break;
        }
    }

    if (!hasSuspended) {
        printf("No suspended jobs to kill.\n");
        return;
    }

    // Prompt the user to select a suspended job
    printf("Suspended jobs: ");
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].status == 3) {
            printf("%d ", i);
        }
    }
    printf("\nEnter the job number to kill: ");

    int selectedJob;
    scanf("%d", &selectedJob);

    // Validate the input
    if (selectedJob < 1 || selectedJob >= MAX_JOBS || jobs[selectedJob].status != 3) {
        printf("Invalid selection.\n");
        return;
    }

    // Kill the selected job
    kill(jobs[selectedJob].pid, SIGKILL);
    jobs[selectedJob].status = 4; // Update status to KILLED
    printf("Killed job %d.\n", selectedJob);
}

// Handling Commands
void handle_command(char cmd) {
    switch (cmd) {
        case 'p':
            print_jobs();  // Function to print the process table
            break;
        case 'r':
            start_new_job();
            // Function to start a new job
            break;
        case 'c':
            continue_suspended_job();
            // Function to continue a suspended job
            break;
        case 'k':
            kill_suspended_job();
            // Function to kill a suspended job
            break;
        case 'h':
            printf("Command :  Action\n");
            printf("   c:      Continue a suspended job\n");
            printf("   h:      Print this help message\n");
            printf("   k:      Kill a suspended job\n");
            printf("   p:      Print the process table\n");
            printf("   q:      Quit\n");
            printf("   r:      Run a new job\n");
            break;
        case 'q':
            // Function to quit the manager
            clean_up_suspended_jobs(); // Ensure handling of any orphaned suspended jobs
            exit(0);
            break;
        default:
            printf("Invalid command. Type 'h' for help.\n");
    }
}


// Main function
int main() {
    initialize_jobs();

    char cmd;
    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    while (1) {
        printf("mgr> ");
        scanf(" %c", &cmd);
        handle_command(cmd);
    }
    return 0;
}
