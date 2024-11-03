#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LEN 512
#define MAXARGS 10
#define ARGLEN 30
#define PROMPT "ELEVENshell:- "

int process_command(char* cmdline);
char** tokenize(char* cmdline);
char* read_cmd(char* prompt, FILE* fp);

int main(){
    char *cmdline;
    char* prompt = PROMPT;   
    while((cmdline = read_cmd(prompt, stdin)) != NULL){
        process_command(cmdline);
        free(cmdline);
    }
    printf("\n");
    return 0;
}
int process_command(char* cmdline){
    char* cmds[MAXARGS];
    int cmd_count = 0;

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
                // Input redirection
                input_redirect = open(arglist[j + 1], O_RDONLY);
                if (input_redirect < 0) {
                    perror("Input file open failed");
                    return -1;
                }
                free(arglist[j]);
                arglist[j] = NULL; // Remove "<" from arglist
            } else if (strcmp(arglist[j], ">") == 0) {
                // Output redirection
                output_redirect = open(arglist[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_redirect < 0) {
                    perror("Output file open failed");
                    return -1;
                }
                free(arglist[j]);
                arglist[j] = NULL; // Remove ">" from arglist
            }
        }

        // If not the last command, create a pipe
        if (i < cmd_count - 1) {
            pipe(fd);
        }

        if (fork() == 0) {
            // Child process

            // Redirect input
            if (input_redirect != -1) {
                dup2(input_redirect, 0); // Redirect stdin to input file
                close(input_redirect);
            } else if (in_fd != 0) {
                dup2(in_fd, 0); // Set stdin to the last pipe’s read end
                close(in_fd);
            }

            // Redirect output
            if (output_redirect != -1) {
                dup2(output_redirect, 1); // Redirect stdout to output file
                close(output_redirect);
            } else if (i < cmd_count - 1) {
                dup2(fd[1], 1); // Set stdout to the pipe’s write end
                close(fd[1]);
            }

            // Execute command
            execvp(arglist[0], arglist);
            perror("exec failed");
            exit(1);
        }

        // Parent process closes used pipe ends and updates in_fd for next command
        if (i < cmd_count - 1) {
            close(fd[1]);
            in_fd = fd[0];
        }

        // Close input/output redirection file descriptors in parent process
        if (input_redirect != -1) {
            close(input_redirect);
        }
        if (output_redirect != -1) {
            close(output_redirect);
        }

        // Free memory for each command
        for(int j = 0; j < MAXARGS + 1; j++) {
            free(arglist[j]);
        }
        free(arglist);
    }

    // Wait for all children to finish
    for (int i = 0; i < cmd_count; i++) {
        wait(0);
    }

    return 0;
}
char** tokenize(char* cmdline){
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

char* read_cmd(char* prompt, FILE* fp){
    printf("%s", prompt);
    int c;
    int pos = 0;
    char* cmdline = (char*)malloc(sizeof(char) * MAX_LEN);
    while((c = getc(fp)) != EOF){
        if(c == '\n')
            break;
        cmdline[pos++] = c;
    }
    if(c == EOF && pos == 0) 
        return NULL;
    cmdline[pos] = '\0';
    return cmdline;
}

