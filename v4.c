#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 512
#define MAXARGS 10
#define PROMPT "PUCITshell:- "
#define HISTORY_SIZE 10

void handle_sigchld(int sig) {
    // Clean up terminated background processes to prevent zombies
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int execute(char* arglist[], int is_background) {
    if (arglist[0] == NULL) {
        fprintf(stderr, "Error: empty command\n");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return -1;
    } else if (pid == 0) {
        // Child process
        execvp(arglist[0], arglist);
        perror("Command execution failed");  // Only if execvp fails
        exit(1);
    } else {
        // Parent process
        if (!is_background) {
            waitpid(pid, NULL, 0);  // Wait for child process if foreground
        } else {
            printf("[Background] Process started with PID %d\n", pid);
        }
    }
    return 0;
}

char** tokenize(char* cmdline, int* background) {
    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    int argnum = 0;

    if (!arglist) {
        fprintf(stderr, "Memory allocation error\n");
        exit(1);
    }

    char* token = strtok(cmdline, " \t\n");
    while (token != NULL && argnum < MAXARGS) {
        if (strcmp(token, "&") == 0) {
            *background = 1;
        } else {
            arglist[argnum++] = strdup(token);
        }
        token = strtok(NULL, " \t\n");
    }
    arglist[argnum] = NULL;  // Null-terminate the array
    return arglist;
}

void handle_redirection_and_pipes(char* cmdline) {
    char* commands[2];
    int pipe_fd[2];
    int num_commands = 0;

    // Split commands by pipe if present
    commands[num_commands] = strtok(cmdline, "|");
    while (commands[num_commands] != NULL && num_commands < 1) {
        commands[++num_commands] = strtok(NULL, "|");
    }

    if (num_commands > 1) {
        // Handle pipe between two commands
        pipe(pipe_fd);
        if (fork() == 0) {
            // First command in the pipe
            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[0]);
            close(pipe_fd[1]);

            int background = 0;
            char** arglist = tokenize(commands[0], &background);
            execute(arglist, background);

            for (int i = 0; arglist[i] != NULL; i++) free(arglist[i]);
            free(arglist);
            exit(0);
        } else {
            // Second command in the pipe
            if (fork() == 0) {
                dup2(pipe_fd[0], STDIN_FILENO);
                close(pipe_fd[1]);
                close(pipe_fd[0]);

                int background = 0;
                char** arglist = tokenize(commands[1], &background);
                execute(arglist, background);

                for (int i = 0; arglist[i] != NULL; i++) free(arglist[i]);
                free(arglist);
                exit(0);
            }
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            wait(NULL);
            wait(NULL);
        }
    } else {
        // Handle redirection if no pipe is present
        char* input_file = NULL;
        char* output_file = NULL;

        if ((input_file = strstr(cmdline, "<")) != NULL) {
            *input_file = '\0';
            input_file = strtok(input_file + 1, " \t\n");
        }

        if ((output_file = strstr(cmdline, ">")) != NULL) {
            *output_file = '\0';
            output_file = strtok(output_file + 1, " \t\n");
        }

        int background = 0;
        char** arglist = tokenize(cmdline, &background);

        pid_t pid = fork();
        if (pid == 0) {
            if (input_file) {
                int in_fd = open(input_file, O_RDONLY);
                if (in_fd < 0) {
                    perror("Input file open failed");
                    exit(1);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if (output_file) {
                int out_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (out_fd < 0) {
                    perror("Output file open failed");
                    exit(1);
                }
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }
            execvp(arglist[0], arglist);
            perror("Command execution failed");
            exit(1);
        } else if (!background) {
            waitpid(pid, NULL, 0);
        }

        for (int i = 0; arglist[i] != NULL; i++) free(arglist[i]);
        free(arglist);
    }
}

void save_history(const char* cmd) {
    add_history(cmd);
    if (history_length > HISTORY_SIZE) {
        HIST_ENTRY** hist_list = history_list();
        if (hist_list && hist_list[0]) {
            free_history_entry(remove_history(0));
        }
    }
}

int main() {
    char* cmdline;
    signal(SIGCHLD, handle_sigchld);  // Handle SIGCHLD to prevent zombies
    using_history();

    while ((cmdline = readline(PROMPT)) != NULL) {
        if (strlen(cmdline) > 0) {
            save_history(cmdline);

            if (cmdline[0] == '!') {
                // Handle history recall (!number)
                int history_index = atoi(cmdline + 1);
                if (history_index > 0 && history_index <= history_length) {
                    free(cmdline);
                    cmdline = strdup(history_get(history_index)->line);
                    printf("Executing from history: %s\n", cmdline);
                } else {
                    printf("No such history entry: %s\n", cmdline);
                    continue;
                }
            }

            handle_redirection_and_pipes(cmdline);
        }
        free(cmdline);
    }
    return 0;
}

