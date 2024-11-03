#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 512
#define MAXARGS 10
#define MAX_VARS 100
#define HISTORY_SIZE 10
#define PROMPT "ELEVENshell:- "

struct var {
    char *name;
    char *value;
    int global;
};

struct var vars[MAX_VARS];
int var_count = 0;
int background_jobs[HISTORY_SIZE];
int job_count = 0;

// Set variable
int set_variable(char *name, char *value, int global) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            free(vars[i].value);
            vars[i].value = strdup(value);
            vars[i].global = global;
            return 0;
        }
    }
    if (var_count < MAX_VARS) {
        vars[var_count].name = strdup(name);
        vars[var_count].value = strdup(value);
        vars[var_count].global = global;
        var_count++;
        return 0;
    }
    fprintf(stderr, "Error: variable limit reached\n");
    return -1;
}

// Retrieve variable
char* get_variable(char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            return vars[i].value;
        }
    }
    return getenv(name);
}

// Unset variable
int unset_variable(char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            free(vars[i].name);
            free(vars[i].value);
            vars[i] = vars[--var_count];
            return 0;
        }
    }
    return unsetenv(name);
}

// Print all user-defined variables
void printvars() {
    for (int i = 0; i < var_count; i++) {
        printf("%s=%s\n", vars[i].name, vars[i].value);
    }
}

// Print environment variables
void printenv_vars() {
    extern char **environ;
    for (char **env = environ; *env != 0; env++) {
        printf("%s\n", *env);
    }
}

// List background jobs
void list_jobs() {
    printf("Background jobs:\n");
    for (int i = 0; i < job_count; i++) {
        printf("[%d] %d\n", i + 1, background_jobs[i]);
    }
}

// Kill a background job by PID
void kill_job(int job_num) {
    if (job_num > 0 && job_num <= job_count) {
        kill(background_jobs[job_num - 1], SIGKILL);
        printf("Job %d killed\n", background_jobs[job_num - 1]);
        for (int i = job_num - 1; i < job_count - 1; i++) {
            background_jobs[i] = background_jobs[i + 1];
        }
        job_count--;
    } else {
        fprintf(stderr, "Invalid job number\n");
    }
}

// Execute command with redirection and background
int execute_command(char **arglist, int background) {
    pid_t pid = fork();
    if (pid == 0) {
        if (background) {
            setpgid(0, 0); // Prevent background job from receiving signals
        }

        // Handle redirection
        for (int i = 0; arglist[i]; i++) {
            if (strcmp(arglist[i], "<") == 0) {
                int fd = open(arglist[i + 1], O_RDONLY);
                dup2(fd, STDIN_FILENO);
                close(fd);
                arglist[i] = NULL;
            } else if (strcmp(arglist[i], ">") == 0) {
                int fd = open(arglist[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                dup2(fd, STDOUT_FILENO);
                close(fd);
                arglist[i] = NULL;
            }
        }
        execvp(arglist[0], arglist);
        perror("Command execution failed");
        exit(1);
    } else {
        if (background) {
            background_jobs[job_count++] = pid;
            printf("Background job started with PID %d\n", pid);
        } else {
            waitpid(pid, NULL, 0);
        }
    }
    return 0;
}

// Parse and execute command line
int parse_and_execute(char *cmdline) {
    if (strncmp(cmdline, "cd ", 3) == 0) {
        chdir(cmdline + 3);
    } else if (strcmp(cmdline, "exit") == 0) {
        exit(0);
    } else if (strcmp(cmdline, "jobs") == 0) {
        list_jobs();
    } else if (strncmp(cmdline, "kill ", 5) == 0) {
        int job_num = atoi(cmdline + 5);
        kill_job(job_num);
    } else if (strcmp(cmdline, "help") == 0) {
        printf("Available commands:\ncd, exit, jobs, kill, set, export, unset, printvars, printenv\n");
    } else if (strncmp(cmdline, "set ", 4) == 0) {
        char *name = strtok(cmdline + 4, " ");
        char *value = strtok(NULL, " ");
        set_variable(name, value, 0);
    } else if (strncmp(cmdline, "export ", 7) == 0) {
        char *name = strtok(cmdline + 7, " ");
        for (int i = 0; i < var_count; i++) {
            if (strcmp(vars[i].name, name) == 0) {
                setenv(vars[i].name, vars[i].value, 1);
                vars[i].global = 1;
                return 0;
            }
        }
        fprintf(stderr, "Variable %s not found\n", name);
    } else {
        char *arglist[MAXARGS + 1];
        char *token = strtok(cmdline, " \t\n");
        int i = 0;
        int background = 0;
        
        while (token && i < MAXARGS) {
            if (strcmp(token, "&") == 0) {
                background = 1;
                break;
            }
            arglist[i++] = token;
            token = strtok(NULL, " \t\n");
        }
        arglist[i] = NULL;

        execute_command(arglist, background);
    }
    return 0;
}

int main() {
    using_history();
    char *cmdline;

    while ((cmdline = readline(PROMPT)) != NULL) {
        if (strlen(cmdline) > 0) {
            add_history(cmdline);
            parse_and_execute(cmdline);
        }
        free(cmdline);
    }
    return 0;
}

