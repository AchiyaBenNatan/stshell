#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_ARGS 10
#define MAX_CMD_LENGTH 100
#define MAX_PIPES 3


void parse_input(char* input, char* args[MAX_ARGS]) {
    int i = 0;
    char* token = strtok(input, " \t\n");
    while (token != NULL) {
        args[i] = token;
        i++;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
}
void parse_input_pipe(char* input, char* args[MAX_ARGS]) {
    int i = 0;
    char* token = strtok(input, "|");
    while (token != NULL) {
        args[i] = token;
        i++;
        token = strtok(NULL, "|");
    }
    args[i] = NULL;
}
void execute_pipeline(char* args[MAX_ARGS], int n_args) {
    int i;
    char* cmds[MAX_PIPES][MAX_ARGS];
    int n_cmds = 0;
    int cmd_start = 0;

    // Parse the commands in the pipeline
    for (i = 0; i < n_args; i++) {
        if (strcmp(args[i], "|") == 0) {
            if (n_cmds >= MAX_PIPES) {
                fprintf(stderr, "Error: too many pipes\n");
                return;
            }
            args[i] = NULL;
            parse_input(cmd_start == 0 ? args[0] : args[cmd_start], cmds[n_cmds]);
            n_cmds++;
            cmd_start = i + 1;
        }
    }
    if (cmd_start < n_args) {
        if (n_cmds >= MAX_PIPES) {
            fprintf(stderr, "Error: too many pipes\n");
            return;
        }
        parse_input(args[cmd_start], cmds[n_cmds]);
        n_cmds++;
    }

    // Execute the pipeline
    int input_fd = STDIN_FILENO;
    int output_fd = STDOUT_FILENO;
    int pipe_fds[MAX_PIPES][2];
    for (i = 0; i < n_cmds; i++) {
        if (i < n_cmds - 1) {
            if (pipe(pipe_fds[i]) < 0) {
                perror("pipe");
                return;
            }
            output_fd = pipe_fds[i][1];
        } else {
            output_fd = STDOUT_FILENO;
        }
        execute_command(cmds[i], input_fd, output_fd);
        if (i > 0) {
            close(pipe_fds[i-1][1]);
        }
        input_fd = pipe_fds[i][0];
    }
    for (i = 0; i < n_cmds - 1; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }
}


void execute_command(char* args[MAX_ARGS], int input_fd, int output_fd) {
    // Handle built-in commands
    // if (strcmp(args[0], "cd") == 0) {
    //     if (args[1] == NULL) {
    //         // No argument provided to cd, go to home directory
    //         chdir(getenv("HOME"));
    //     } else {
    //         if (chdir(args[1]) != 0) {
    //             fprintf(stderr, "cd: %s: No such file or directory\n", args[1]);
    //         }
    //     }
    //     return;
    // }

    // Fork to execute command
    pid_t pid = fork();

    if (pid == 0) {  // Child process
        // Set signal handler for SIGINT (Ctrl+C)
        signal(SIGINT, SIG_DFL);

        // Redirect input/output if necessary
        if (input_fd != STDIN_FILENO) {
            if (dup2(input_fd, STDIN_FILENO) < 0) {
                perror("dup2");
                exit(1);
            }
            close(input_fd);
        }
        if (output_fd != STDOUT_FILENO) {
            if (dup2(output_fd, STDOUT_FILENO) < 0) {
                perror("dup2");
                exit(1);
            }
            close(output_fd);
        }

        // Execute command
        execvp(args[0], args);
        perror("execvp");
        exit(1);
    } else if (pid < 0) {
        perror("fork");
        exit(1);
    }

    // Wait for child process to complete
    int status;
    wait(&status);
}

void execute_input(char* input) {
    //add_to_history(input);
    char* args[MAX_ARGS];
    parse_input(input, args);
    int n_args = 0;
    while (args[n_args] != NULL) {
        n_args++;
    }
    if (n_args == 0) {
        return;
    }
    if (strcmp(args[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    } else {
        execute_pipeline(args, n_args);
    }
}

int main() {
    char input[1024];
    char* args[MAX_ARGS];
    int n_args;
    int output_fd = STDOUT_FILENO;
    int sigint_flag = 0;
    
    for (size_t i = 0; i < MAX_ARGS; i++)
    {
        args[i] = NULL;
    }
    
    signal(SIGINT, SIG_IGN);

    while (1) {
        // Print the shell prompt
        printf("stshell> ");

        // Read a line of input
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (sigint_flag) {
                sigint_flag = 0;
                continue;
            }
            break;
        }

        // Check for exit command
        if (strcmp(input, "exit\n") == 0) {
            break;
        }

        // Tokenize the input into arguments
        n_args = 0;
        args[n_args] = strtok(input, " \t\n|");
        while (args[n_args] != NULL) {
            n_args++;
            args[n_args] = strtok(NULL, " \n");
        }
        // Check for output redirection
        for (int i = 0; i < n_args - 1; i++) {
            if (strcmp(args[i], ">") == 0) {
                output_fd = open(args[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd < 0) {
                    perror("open");
                    output_fd = STDOUT_FILENO;
                    break;
                }
                memmove(args+i, args+i+2, (n_args-i-1)*sizeof(char*));
                n_args -= 2;
                i--;
            } else if (strcmp(args[i], ">>") == 0) {
                output_fd = open(args[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (output_fd < 0) {
                    perror("open");
                    output_fd = STDOUT_FILENO;
                    break;
                }
                memmove(args+i, args+i+2, (n_args-i-1)*sizeof(char*));
                n_args -= 2;
                i--;
            }
        }

        // Check for input redirection
        for (int i = 0; i < n_args - 1; i++) {
            if (strcmp(args[i], "<") == 0) {
                int input_fd = open(args[i+1], O_RDONLY);
                if (input_fd < 0) {
                    perror("open");
                    break;
                }
                memmove(args+i, args+i+2, (n_args-i-1)*sizeof(char*));
                n_args -= 2;
                i--;
                execute_command(args, input_fd, output_fd);
                close(input_fd);
                output_fd = STDOUT_FILENO;
                break;
            }
        }

        // Check for pipeline
        for (int i = 0; i < n_args; i++) {
            
            if (strcmp(args[i], "|") == 0) {
                execute_pipeline(args,n_args);
                output_fd = STDOUT_FILENO;
                break;
            }
        }

        // Execute the command
        if (n_args > 0) {
            execute_command(args, STDIN_FILENO, output_fd);
            output_fd = STDOUT_FILENO;
        }
    }

    return 0;
}