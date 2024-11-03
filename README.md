# The UNIX Shell

This project is my own personal UNIX shell implemented in C, designed to mimic the functionality of standard UNIX shell while gradually building advanced features over multiple versions. This shell supports executing commands, I/O redirection, background processes, history, built-in commands, and variable management. It was developed as part of my Operating Systems Course (Fall 2024)


## Versions and Functionalities

### v1: Basic Command Execution
- **Functionality**: Executes standard system commands.
- **Commands**: `ls`, `cat`, `pwd`, etc.
- **Details**: 
  - Uses `fork()` and `execvp()` to execute commands.
  - Returns the prompt after command execution completes.

### v2: Input and Output Redirection
- **Functionality**: Adds support for redirecting `stdin` and `stdout` using `<` and `>` and command piping (`|`) to chain commands together.
- **Example Usage**:
  ```shell
  cat < infile.txt > outfile.txt
  ```
  ```shell
  cat file.txt | wc -l
  ```
- **Details**:
  - Opens `infile.txt` in read-only mode, redirects `stdin`.
  - Opens `outfile.txt` in write-only mode, redirects `stdout`.
  - Uses `pipe()` and `dup2()` to connect the output of one command to the input of the next.

### v3: Background Process Execution
- **Functionality**: Adds support for running processes in the background using `&`.
- **Example Usage**:
  ```shell
  long_running_command &
  ```
- **Details**:
  - Starts the command in the background, returning the prompt immediately.
  - Manages background jobs to prevent zombie processes.

### v4: Command History
- **Functionality**: Maintains a history of the last 10 commands, enabling repeat commands with `!number` as well as repeating commands using arrow keys.
- **Example Usage**:
  ```shell
  !1    # Executes the first command in history
  ^ key  # Executes the last command in history
  ```
- **Details**:
  - Uses `readline` and `history` libraries to store and retrieve command history.

### v5: Built-in Commands
- **Functionality**: Adds built-in commands that are executed directly in the shell without forking a new process.
- **Built-in Commands**:
  - `cd <directory>`: Change directory.
  - `exit`: Exit the shell.
  - `jobs`: List background jobs.
  - `kill <job_number>`: Kill a background job by job number.
  - `help`: List available built-in commands and their syntax.

### v6: Variable Management
- **Functionality**: Adds support for user-defined and environment variables.
- **Commands**:
  - `set <var> <value>`: Define a local variable.
  - `export <var>`: Make a local variable an environment variable.
  - `unset <var>`: Remove a variable.
  - `printvars`: Display user-defined variables.
  - `printenv`: Display environment variables.

## How to Use

1. **Compile the Shell**:
   ```bash
   gcc <version.c> -o shell -lreadline
   ```
2. **Run the Shell**:
   ```bash
   ./shell
   ```
3. **Execute Commands**:
   - Type any UNIX command directly in the shell to execute it.

