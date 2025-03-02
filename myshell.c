#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

// Initializes signal handling for SIGINT (Ctrl+C)
int prepare(void){
    struct sigaction sa;
    
    sa.sa_handler = SIG_IGN; // Ignore SIGINT (Ctrl+C) in the parent shell
    sa.sa_flags = SA_RESTART; // Restart system calls after the signal
    sigfillset(&sa.sa_mask); // Block all signals during the handler

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction failed");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

// Handles piping between two commands in a single process chain
void execute_with_pipe(char** arglist, int pipe_pos, int count) {
    int fd[2];
    pid_t pid1, pid2;
    struct sigaction sa;
    sa.sa_handler = SIG_DFL; // Default signal handler for child processes

    if (pipe(fd) == -1) { // Child process 1 (before the pipe)
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
                perror("sigaction failed");
                exit(EXIT_FAILURE);
            }
        close(fd[0]); // Close reading end of pipe
        dup2(fd[1], STDOUT_FILENO); // Redirect stdout to the pipe
        close(fd[1]);

        arglist[pipe_pos] = NULL;
        execvp(arglist[0], arglist); // Execute the first command
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else { // Parent process
        // Parent process
        pid2 = fork();
        if (pid2 == -1) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        }

        if (pid2 == 0) { // Child process 2 (after the pipe)
            // Child process 2
            if (sigaction(SIGINT, &sa, NULL) == -1) {
                perror("sigaction failed");
                exit(EXIT_FAILURE);
            }
            close(fd[1]);  // Close writing end of pipe
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);

            execvp(arglist[pipe_pos + 1], &arglist[pipe_pos + 1]); // Execute second command
            perror("execvp failed");
            exit(EXIT_FAILURE);
        } else { // Parent process waits for both children
            // Parent process
            close(fd[0]);
            close(fd[1]);
            waitpid(pid1, NULL, 0); // Wait for first child
            waitpid(pid2, NULL, 0); // Wait for second child
        }
    }
}

// Handles input/output redirection and executes the command
void redirect_type_and_execute(char **arglist, const char *file, int type) {
    int fd;
    if (type == 0){
        fd = open(file, O_RDONLY); // Input redirection
    }
    else{
        fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Output redirection
    }
    if (fd == -1) {
        perror("Failed to open input file");
        return;
    }
    int copy = dup(type); // Save the original file descriptor

    if (dup2(fd, type) == -1) { // Redirect stdin/stdout
        perror("Failed to redirect standard input");
        close(fd);
        return;
    }
    close(fd);

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) { // Child process
        execvp(arglist[0], arglist); // Execute the command
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else { // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for child to finish
        dup2(copy, type); // Restore original file descriptor
        close(copy);
    }
}

// Processes arguments, handles background execution, pipes, and redirection
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
 
    for (int i = 0; i < count; i++) {
         //Check if the arglist contain is "|", and execute pipe 
        if (strcmp(arglist[i], "|") == 0) {
            execute_with_pipe(arglist, i, count);
            return 1;
        }
        //Check if the arglist contain is "<", and execute Input redirecting 
        if (strcmp(arglist[i], "<") == 0 && strcmp(arglist[i+1], "<") != 0) {
            arglist[i] = NULL;
            redirect_type_and_execute(arglist, arglist[i+1], 0);
            return 1;
        }
        //Check if the arglist contain is ">", and execute Output redirecting
        if (strcmp(arglist[i], ">") == 0 && strcmp(arglist[i+1], ">") != 0) {
            arglist[i] = NULL;
            redirect_type_and_execute(arglist, arglist[i+1], 1);
            return 1;
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
                perror("sigaction failed");
                exit(EXIT_FAILURE);
            }
        }
        execvp(arglist[0], arglist);     // Execute the command
        perror("execvp failed");
        exit(EXIT_FAILURE);
    } else {
        if (!background) {
            int status;
            waitpid(pid, &status, 0);    // Wait for child to finish
            if (pid == -1 && errno != ECHILD && errno != EINTR) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }
        }
    }
    return 1; 
}
// Final cleanup function, waits for all child processes
int finalize(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) { }

    if (pid == -1 && errno != ECHILD && errno != EINTR) {
        perror("waitpid");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
