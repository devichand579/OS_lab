#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define READ_END 0
#define WRITE_END 1 
#define BUFFER_SIZE 256


void c_child_loop(int fd0, int fd1, int fd3, int fd4) {
    char command[BUFFER_SIZE];
    int role =0;
        if(role == 0){
           dup2(fd1, STDOUT_FILENO);
           close(fd0);
           fprintf(stderr, "Enter command > ");
           fflush(stderr);
           while(1){
           if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
              break;
           }
           if (strncmp(command, "exit", 4) == 0) {
              printf("exit\n");
              fflush(stdout);
              break;
            }
            if (strncmp(command, "swaprole", 8) == 0) {
                printf("swaprole\n");
                role =1;
                fflush(stdout);
                continue;
            }
            printf("%s", command);
            fflush(stdout);
            }
        }
       else if(role ==1){
              dup2(fd3, STDIN_FILENO);
              close(fd4);
              fprintf(stderr, "Waiting for command > ");
              fflush(stderr);
                while(1){
            if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
              break;
            }
            if (strncmp(command, "exit", 4) == 0) {
               break;
            }
            if (strncmp(command, "swaprole", 8) == 0) {
                // Switch to C role
                role =0;
                continue;
            }
            pid_t pid = fork();
            if (pid == 0) {
              printf("%s\n", command);
              execlp("sh", "sh", "-c", command, (char *)NULL);
              perror("execlp");
              exit(EXIT_FAILURE);
            } else {
              wait(NULL);

       }
       }
    
   }
   close(fd1);
   close(fd3);
}

void e_child_loop(int fd0, int fd1, int fd3, int fd4) {
    char command[BUFFER_SIZE];
    int role =1;
            if (role == 1){
                 dup2(fd0, STDIN_FILENO);
                close(fd1);
                fprintf(stderr, "Waiting for command > ");
                fflush(stderr);
                while(1){
            if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
                break;
            }
            if (strncmp(command, "exit", 4) == 0) {
                break;
            }
              if (strncmp(command, "swaprole", 8) == 0) {
                  role =0;
                  continue;
              }
              pid_t pid = fork();
              if (pid == 0) {
                 printf("%s\n", command);
                 execlp("sh", "sh", "-c", command, (char *)NULL);
                 perror("execlp");
                 exit(EXIT_FAILURE);
              } else {
                wait(NULL);
              }
          }
        }
        else if (role == 0){
                dup2(fd4, STDOUT_FILENO);
                close(fd3);
                fprintf(stderr, "Enter command > ");
                fflush(stderr);
                while(1){
                if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
                    break;
                }
                if (strncmp(command, "exit", 4) == 0) {
                    printf("exit\n");
                    fflush(stdout);
                    break;
                }
                if (strncmp(command, "swaprole", 8) == 0) {
                    printf("swaprole\n");
                    role =1;
                    fflush(stdout);
                    continue;
                }
                printf("%s", command);
                fflush(stdout);
          }
    }
    close(fd0);
    close(fd4);
}

int main(int argc, char *argv[]) {
    int pipe_c_to_e[2];
    int pipe_e_to_c[2];
    pid_t pid_c, pid_e;

    if (argc == 1) {
        if (pipe(pipe_c_to_e) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
        if (pipe(pipe_e_to_c) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        char fd0_str[10], fd1_str[10];
        sprintf(fd0_str, "%d", pipe_c_to_e[READ_END]);
        sprintf(fd1_str, "%d", pipe_c_to_e[WRITE_END]);

        char fd3_str[10], fd4_str[10];
        sprintf(fd3_str, "%d", pipe_e_to_c[READ_END]);
        sprintf(fd4_str, "%d", pipe_e_to_c[WRITE_END]);

        pid_c = fork();
        if (pid_c < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid_c == 0) {
            execlp("xterm", "xterm", "-T", "Child C", "-e", "./CSE", "C", fd0_str, fd1_str,fd3_str,fd4_str, NULL);
            perror("execlp");
            exit(EXIT_FAILURE);
        } else {
            pid_e = fork();
            if (pid_e < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            if (pid_e == 0) {
                execlp("xterm", "xterm", "-T", "Child E", "-e", "./CSE", "E", fd0_str, fd1_str,fd3_str,fd4_str, NULL);
                perror("execlp");
                exit(EXIT_FAILURE);
            } else {
                close(pipe_c_to_e[WRITE_END]);
                close(pipe_c_to_e[READ_END]);
                close(pipe_e_to_c[WRITE_END]);
                close(pipe_e_to_c[READ_END]);
                waitpid(pid_c, NULL, 0);
                waitpid(pid_e, NULL, 0);
            }
        }
    } else {
    
        if (strcmp(argv[1], "C") == 0) {
            c_child_loop(atoi(argv[2]), atoi(argv[3]),atoi(argv[4]),atoi(argv[5]));
        } else if (strcmp(argv[1], "E") == 0) {
            e_child_loop(atoi(argv[2]), atoi(argv[3]),atoi(argv[4]),atoi(argv[5]));
    }

}
   return 0;
}
