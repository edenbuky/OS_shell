# MyShell

MyShell is a simple implementation of a shell program written in C. The shell supports basic functionalities like executing commands, background processes, piping, and input/output redirection. The assignment's goal was to gain experience with process management, pipes, signals, and system calls.

## Features

- **Executing commands**: Runs commands like `sleep 10`.
- **Background execution**: Runs commands in the background using the `&` symbol.
- **Piping**: Supports piping between two commands using the `|` symbol.
- **Input redirection**: Supports redirecting input from a file using `<`.
- **Output redirection**: Supports redirecting output to a file using `>`.

## File Structure

- **myshell.c**: Contains the main functionality of the shell.
- **shell.c**: Skeleton code provided by the assignment that interacts with the shell.

## Functions

### `int prepare(void)`

This function is called before the first invocation of `process_arglist()`. It's used for any necessary setup or initialization before running commands.

- **Usage**: Set up signal handling (e.g., ignore SIGINT).
- **Return**: Returns `0` on success, and non-zero on failure.

### `int process_arglist(int count, char **arglist)`

This function processes the list of arguments received from the command line input. It handles executing commands, background processes, pipes, and input/output redirection.

- **Parameters**:
  - `count`: The number of arguments in `arglist`.
  - `arglist`: The list of arguments (words) parsed from the user’s input.
  
- **Key Operations**:
  - **Background Execution**: If the command ends with `&`, the shell will execute it in the background.
  - **Piping**: If the command includes `|`, it pipes the output of the first command to the second.
  - **Input Redirection**: If the command contains `<`, the shell redirects input from the specified file.
  - **Output Redirection**: If the command contains `>`, the shell redirects output to the specified file.

- **Return**: Returns `1` to continue execution of the shell, or `0` if an error occurs.

### `int finalize(void)`

This function is called before the shell exits. It's used to clean up any processes or resources initialized in the `prepare()` or `process_arglist()` functions.

- **Usage**: Handle any necessary cleanup.
- **Return**: Returns `0` on success, and non-zero on failure.

## Error Handling

The program ensures proper error messages for the following scenarios:

- Errors during system calls like `fork()`, `execvp()`, `pipe()`, and file operations.
- Background processes do not stop on SIGINT, while foreground processes do.
- Errors related to invalid input, like non-existing programs or redirection failures, are handled gracefully within child processes.

## Compilation

To compile the program, use the following command:

```bash
gcc -O3 -D_POSIX_C_SOURCE=200809 -Wall -std=c11 shell.c myshell.c -o myshell
