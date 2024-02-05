#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>


int prepare(void){
    struct sigaction sa;
    
    sa.sa_handler = SIG_IGN;
    sigfillset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void execute_with_pipe(char** arglist, int pipe_pos, int count) {
    int fd[2];
    pid_t pid1, pid2;
    struct sigaction sa;
    sa.sa_handler = SIG_DFL;

    if (pipe(fd) == -1) {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }

    pid1 = fork();
    if (pid1 == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0) {
        // Child process 1
        if (sigaction(SIGINT, &sa, NULL) == -1) {
                perror("sigaction");
                exit(EXIT_FAILURE);
            }
        close(fd[0]); // Close unused read end
        dup2(fd[1], STDOUT_FILENO); // Redirect stdout to pipe write end
        close(fd[1]);

        arglist[pipe_pos] = NULL; // Split the arglist at the pipe position
        execvp(arglist[0], arglist);
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        pid2 = fork();
        if (pid2 == -1) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid2 == 0) {
            // Child process 2
            if (sigaction(SIGINT, &sa, NULL) == -1) {
                perror("sigaction");
                exit(EXIT_FAILURE);
            }
            close(fd[1]); // Close unused write end
            dup2(fd[0], STDIN_FILENO); // Redirect stdin to pipe read end
            close(fd[0]);

            execvp(arglist[pipe_pos + 1], &arglist[pipe_pos + 1]);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            close(fd[0]);
            close(fd[1]);
            waitpid(pid1, NULL, 0); // Wait for first child to finish
            waitpid(pid2, NULL, 0); // Wait for second child to finish
        }
    }
}
void redirect_input(const char* file) {
    int fd = open(file, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open input file");
        exit(EXIT_FAILURE);
    }
    if (dup2(fd, STDIN_FILENO) == -1) {
        perror("Failed to redirect standard input");
        exit(EXIT_FAILURE);
    }
    close(fd);
}

void redirect_output(const char* file) {
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("Failed to open output file");
        exit(EXIT_FAILURE);
    }
    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("Failed to redirect standard output");
        exit(EXIT_FAILURE);
    }
    close(fd);
}

int process_arglist(int count, char** arglist) {
    int background = 0;
    pid_t pid;
    struct sigaction sa_default;
    struct sigaction sa_ignore;

    sa_default.sa_handler = SIG_DFL;
    sigemptyset(&sa_default.sa_mask);
    sa_default.sa_flags = 0;

    sa_ignore.sa_handler = SIG_IGN;
    sigfillset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;

    // Check if the last argument is "&", indicating background execution
    if (strcmp(arglist[count-1], "&") == 0) {
        background = 1;
        arglist[count-1] = NULL; // Remove "&" from the arguments
        count--; // Adjust count to ignore "&"
    }

    //Check if the arglist contain is "|" 
    for (int i = 0; i < count; i++) {
         //Check if the arglist contain is "|", and execute pipe 
        if (strcmp(arglist[i], "|") == 0) {
            execute_with_pipe(arglist, i, count);
            return 1;
        }
        //Check if the arglist contain is "<", and execute Input redirecting 
        if (strcmp(arglist[i], "<") == 0 && strcmp(arglist[i+1], "<") != 0) {
            redirect_input(arglist[i+1]);
            arglist[i] = NULL;
            break;
        }
        //Check if the arglist contain is ">", and execute Output redirecting.
        if (strcmp(arglist[i], ">") == 0 && strcmp(arglist[i+1], ">") != 0) {
            redirect_output(arglist[i+1]);
            arglist[i] = NULL;
            break;
        }
    }
    
    pid = fork();

    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } 
    else if (pid == 0) {        // Child process
        if (!background) {
            if (sigaction(SIGINT, &sa_default, NULL) == -1) {
                perror("sigaction");
                exit(EXIT_FAILURE);
            }
        }
        execvp(arglist[0], arglist);
        perror("execvp failed"); // execvp only returns on error
        exit(EXIT_FAILURE);
    } else {
        if (!background) {
            int status;
            // if (sigaction(SIGINT, &sa_ignore, NULL) == -1) {
            //     perror("sigaction");
            //     exit(EXIT_FAILURE);
            // }
            waitpid(pid, &status, 0); // Wait for the child to finish
        }
    }
    return 1; // Indicate success
}

int finalize(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Optionally, print out the PID of each child process that was reaped
        printf("Reaped child process PID: %d\n", pid);
    }

    if (pid == -1 && errno != ECHILD) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
