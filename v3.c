#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "ELEVENshell:- "

int process_command(char* cmdline);
char** tokenize(char* cmdline);
char* read_cmd(char* prompt, FILE* fp);
void handle_sigchld(int sig);

int main() {
    char *cmdline;
    char* prompt = PROMPT;

    // Set up the SIGCHLD handler to prevent zombie processes
    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    while((cmdline = read_cmd(prompt, stdin)) != NULL) {
        process_command(cmdline);
        free(cmdline);
    }
    printf("\n");
    return 0;
}

void handle_sigchld(int sig) {
    // Clean up all child processes that have terminated
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int process_command(char* cmdline) {
    char* cmds[MAXARGS];
    int cmd_count = 0;
    int is_background = 0;

    // Check for background process indicator '&'
    int len = strlen(cmdline);
    if (cmdline[len - 1] == '&') {
        is_background = 1;
        cmdline[len - 1] = '\0'; // Remove '&' from the command
    }

    // Split the command line by the pipe symbol "|"
    char* token = strtok(cmdline, "|");
    while (token != NULL && cmd_count < MAXARGS) {
        cmds[cmd_count++] = token;
        token = strtok(NULL, "|");
    }

    int in_fd = 0; // Initial input is stdin
    int fd[2];

    // Loop through each command
    for (int i = 0; i < cmd_count; i++) {
        char** arglist = tokenize(cmds[i]);

        // Variables for input and output redirection
        int input_redirect = -1;
        int output_redirect = -1;

        // Check for input/output redirection in arguments
        for (int j = 0; arglist[j] != NULL; j++) {
            if (strcmp(arglist[j], "<") == 0) {
                input_redirect = open(arglist[j + 1], O_RDONLY);
                if (input_redirect < 0) {
                    perror("Input file open failed");
                    return -1;
                }
                free(arglist[j]);
                arglist[j] = NULL;
            } else if (strcmp(arglist[j], ">") == 0) {
                output_redirect = open(arglist[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_redirect < 0) {
                    perror("Output file open failed");
                    return -1;
                }
                free(arglist[j]);
                arglist[j] = NULL;
            }
        }

        // If not the last command, create a pipe
        if (i < cmd_count - 1) {
            pipe(fd);
        }

        pid_t pid = fork();
        if (pid == 0) {
            // Child process

            // Redirect input
            if (input_redirect != -1) {
                dup2(input_redirect, 0);
                close(input_redirect);
            } else if (in_fd != 0) {
                dup2(in_fd, 0);
                close(in_fd);
            }

            // Redirect output
            if (output_redirect != -1) {
                dup2(output_redirect, 1);
                close(output_redirect);
            } else if (i < cmd_count - 1) {
                dup2(fd[1], 1);
                close(fd[1]);
            }

            execvp(arglist[0], arglist);
            perror("exec failed");
            exit(1);
        }

        if (!is_background) {
            // Wait for the foreground process to finish
            waitpid(pid, NULL, 0);
        }

        // Close pipe ends and update in_fd
        if (i < cmd_count - 1) {
            close(fd[1]);
            in_fd = fd[0];
        }

        if (input_redirect != -1) {
            close(input_redirect);
        }
        if (output_redirect != -1) {
            close(output_redirect);
        }

        // Free memory for each command
        for (int j = 0; j < MAXARGS + 1; j++) {
            free(arglist[j]);
        }
        free(arglist);
    }

    if (is_background) {
        printf("[Background] Process started with PID %d\n", getpid());
    }

    return 0;
}

char** tokenize(char* cmdline) {
    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    int argnum = 0;

    char* token = strtok(cmdline, " \t\n");
    while (token != NULL && argnum < MAXARGS) {
        arglist[argnum] = (char*)malloc(strlen(token) + 1);
        strcpy(arglist[argnum], token);
        argnum++;
        token = strtok(NULL, " \t\n");
    }
    arglist[argnum] = NULL;

    return arglist;
}

char* read_cmd(char* prompt, FILE* fp) {
    printf("%s", prompt);
    int c;
    int pos = 0;
    char* cmdline = (char*)malloc(sizeof(char) * MAX_LEN);
    while((c = getc(fp)) != EOF) {
        if(c == '\n')
            break;
        cmdline[pos++] = c;
    }
    if(c == EOF && pos == 0) 
        return NULL;
    cmdline[pos] = '\0';
    return cmdline;
}

