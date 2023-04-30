#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include "unistd.h"
#include <string.h>
#include <signal.h>

#define OUT 0
#define APP 1
#define COMMAND_SIZE 1024
#define LINE_COMMAND_SIZE 50

#define TRUE 1

#define END_L_STR "\0"
#define END_L_CHR '\0'

#define EXIT "exit"
#define SHELLPROMPT "stshell>"

#define PIPE_STR "|"
#define PIPE_CHR '|'
#define EMPTY_STRING " "
#define EMPTY_CHAR ' '
#define STDOUT_CHR '>'
#define APPEND_STR ">>"

int status;
char *prompt = SHELLPROMPT;
int parse_command(char **parsed_command, char *cmd, const char *delimeter)
{
    char *token;
    token = strtok(cmd, delimeter);
    int counter = -1;

    while (token)
    {
        parsed_command[++counter] = malloc(strlen(token) + 1);
        strcpy(parsed_command[counter], token);
        if (strcmp(delimeter, PIPE_STR) == 0)
        {
            if (parsed_command[counter][strlen(token) - 1] == EMPTY_CHAR)
            {
                parsed_command[counter][strlen(token) - 1] = END_L_CHR;
            }
            if (parsed_command[counter][0] == EMPTY_CHAR)
            {
                memmove(parsed_command[counter], parsed_command[counter] + 1, strlen(token));
            }
        }
        parsed_command[counter][strlen(token) + 1] = END_L_CHR;
        token = strtok(NULL, delimeter);
    }
    parsed_command[++counter] = NULL;
    return counter;
}

void pipe_tasks(char *cmd)
{
    char *parsed_command[LINE_COMMAND_SIZE];
    int commands = parse_command(parsed_command, cmd, PIPE_STR);
    char *inner_cmd[LINE_COMMAND_SIZE];
    int fd[commands][2];
    for (int i = 0; i < commands; ++i)
    {
        int inner_commands = parse_command(inner_cmd, parsed_command[i], EMPTY_STRING);
        if (inner_commands < 0)
        {
            printf("Error parsing command");
        }

        if (i != commands - 1)
            pipe(fd[i]);
        
        if (fork() == 0)
        {
            if (strchr(inner_cmd[1], STDOUT_CHR) && !strstr(inner_cmd[1], APPEND_STR))
            {
                dup2(fd[i - 1][0], 0);
                int fd_red = creat(inner_cmd[inner_commands - 1], 0660);
                dup2(fd_red, 1);
                close(fd[i - 1][0]);
                close(fd[i - 1][1]);
                inner_cmd[inner_commands - 2] = inner_cmd[inner_commands - 1] = NULL;
                execvp(inner_cmd[0], inner_cmd);
                close(fd_red);
            }
            else if (strstr(inner_cmd[1], APPEND_STR))
            {
                dup2(fd[i - 1][0], 0);
                int fd_red = open(inner_cmd[inner_commands - 1], O_CREAT | O_APPEND | O_RDWR, 0660);
                dup2(fd_red, 1);
                close(fd[i - 1][0]);
                close(fd[i - 1][1]);
                inner_cmd[inner_commands - 2] = inner_cmd[inner_commands - 1] = NULL;
                execvp(inner_cmd[0], inner_cmd);
                close(fd_red);
            }
            else
            {
                if (i != commands - 1)
                {
                    dup2(fd[i][1], 1);
                    close(fd[i][0]);
                    close(fd[i][1]);
                }
                if (i != 0)
                {
                    dup2(fd[i - 1][0], 0);
                    close(fd[i - 1][0]);
                    close(fd[i - 1][1]);
                }
                execvp(inner_cmd[0], inner_cmd);
            }
        }
        if (i != 0)
        {
            close(fd[i - 1][0]);
            close(fd[i - 1][1]);
        }
        wait(NULL); // wait for next command
    }
}
void redirect_tasks(char *command, int direction)
{
    if (fork() == 0)
    { // child
        char *parsed_command[LINE_COMMAND_SIZE];
        int commands = parse_command(parsed_command, command, EMPTY_STRING);
        int fd;
        switch (direction)
        {
        case OUT: // output
            fd = creat(parsed_command[commands - 1], 0660);
            dup2(fd, 1);
            break;

        case APP: // append
            fd = open(parsed_command[commands - 1], O_CREAT | O_APPEND | O_RDWR, 0660);
            dup2(fd, 1);
            break;

        default:
            break;
        }
        parsed_command[commands - 2] = parsed_command[commands - 1] = NULL;
        execvp(parsed_command[0], parsed_command);
    }
    else
    {
        wait(&status);
    }
}

int main()
{
    // handler for Ctrl+C
    signal(SIGINT, SIG_IGN);
    char command[COMMAND_SIZE], saved_cmd[COMMAND_SIZE];
    while (TRUE)
    {
        printf("%s ", prompt);
        fgets(command, COMMAND_SIZE, stdin);
        command[strlen(command) - 1] = END_L_CHR;
        if (!strcmp(command, EXIT))
        {
            break;
        }
        else
        {
            strcpy(saved_cmd, command);
        }
        if (strchr(command, PIPE_CHR))
        {
            pipe_tasks(command);
        }
        else if (strchr(command, STDOUT_CHR) && !strstr(command, APPEND_STR))
        {
            redirect_tasks(command, OUT);
        }
        else if (strstr(command, APPEND_STR))
        {
            redirect_tasks(command, APP);
        }
        else if (fork() == 0)
        {
            char *parsed_command[LINE_COMMAND_SIZE];
            parse_command(parsed_command, command, EMPTY_STRING);
            execvp(parsed_command[0], parsed_command);
        }
        else
        {
            wait(&status);
        }
    }
    return 0;
}