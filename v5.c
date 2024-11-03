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
#define MAX_JOBS 100

typedef struct {
    pid_t pid;
    char cmdline[MAX_LEN];
    int active;
} Job;

Job jobs[MAX_JOBS];
int job_count = 0;

void add_job(pid_t pid, const char* cmdline) {
    if (job_count < MAX_JOBS) {
        jobs[job_count].pid = pid;
        strncpy(jobs[job_count].cmdline, cmdline, MAX_LEN);
        jobs[job_count].active = 1;
        job_count++;
    }
}

void list_jobs() {
    printf("Background jobs:\n");
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active) {
            printf("[%d] PID: %d, Command: %s\n", i + 1, jobs[i].pid, jobs[i].cmdline);
        }
    }
}

void remove_job(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].pid == pid) {
            jobs[i].active = 0;
            break;
        }
    }
}

void handle_sigchld(int sig) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_job(pid);
    }
}

int execute(char* arglist[], int is_background, const char* cmdline) {
    if (arglist[0] == NULL) {
        fprintf(stderr, "Error: empty command\n");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        return -1;
    } else if (pid == 0) {
        execvp(arglist[0], arglist);
        perror("Command execution failed");
        exit(1);
    } else {
        if (!is_background) {
            waitpid(pid, NULL, 0);
        } else {
            printf("[Background] Process started with PID %d\n", pid);
            add_job(pid, cmdline);
        }
    }
    return 0;
}

int is_builtin(char* cmd) {
    return strcmp(cmd, "cd") == 0 || strcmp(cmd, "exit") == 0 || strcmp(cmd, "jobs") == 0 ||
           strcmp(cmd, "kill") == 0 || strcmp(cmd, "help") == 0;
}

void handle_builtin(char** arglist) {
    if (strcmp(arglist[0], "cd") == 0) {
        if (arglist[1] == NULL || chdir(arglist[1]) != 0) {
            perror("cd failed");
        }
    } else if (strcmp(arglist[0], "exit") == 0) {
        printf("Exiting PUCITshell\n");
        exit(0);
    } else if (strcmp(arglist[0], "jobs") == 0) {
        list_jobs();
    } else if (strcmp(arglist[0], "kill") == 0) {
        if (arglist[1] == NULL) {
            fprintf(stderr, "Usage: kill <job_number>\n");
            return;
        }
        int job_num = atoi(arglist[1]);
        if (job_num <= 0 || job_num > job_count || !jobs[job_num - 1].active) {
            fprintf(stderr, "Invalid job number\n");
        } else {
            kill(jobs[job_num - 1].pid, SIGKILL);
            jobs[job_num - 1].active = 0;
        }
    } else if (strcmp(arglist[0], "help") == 0) {
        printf("PUCITshell Built-in Commands:\n");
        printf("cd <directory> : Change the working directory\n");
        printf("exit           : Exit the shell\n");
        printf("jobs           : List background jobs\n");
        printf("kill <job_num> : Kill a background job\n");
        printf("help           : Show this help message\n");
    }
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
    arglist[argnum] = NULL;
    return arglist;
}

void handle_redirection_and_pipes(char* cmdline) {
    char* commands[2];
    int pipe_fd[2];
    int num_commands = 0;

    commands[num_commands] = strtok(cmdline, "|");
    while (commands[num_commands] != NULL && num_commands < 1) {
        commands[++num_commands] = strtok(NULL, "|");
    }

    if (num_commands > 1) {
        pipe(pipe_fd);
        if (fork() == 0) {
            dup2(pipe_fd[1], STDOUT_FILENO);
            close(pipe_fd[0]);
            close(pipe_fd[1]);

            int background = 0;
            char** arglist = tokenize(commands[0], &background);
            execute(arglist, background, commands[0]);

            for (int i = 0; arglist[i] != NULL; i++) free(arglist[i]);
            free(arglist);
            exit(0);
        } else {
            if (fork() == 0) {
                dup2(pipe_fd[0], STDIN_FILENO);
                close(pipe_fd[1]);
                close(pipe_fd[0]);

                int background = 0;
                char** arglist = tokenize(commands[1], &background);
                execute(arglist, background, commands[1]);

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

        if (is_builtin(arglist[0])) {
            handle_builtin(arglist);
        } else {
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
            } else {
                add_job(pid, cmdline);
            }
        }

        for (int i = 0; arglist[i] != NULL; i++) free(arglist[i]);
        free(arglist);
    }
}

int main() {
    char* cmdline;
    signal(SIGCHLD, handle_sigchld);
    using_history();

    while ((cmdline = readline(PROMPT)) != NULL) {
        if (strlen(cmdline) > 0) {
            add_history(cmdline);
            handle_redirection_and_pipes(cmdline);
        }
        free(cmdline);
    }
    return 0;
}

