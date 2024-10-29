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
#define PROMPT "eleven:- "

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
        //token = strtok(NULL, "|");
    }
