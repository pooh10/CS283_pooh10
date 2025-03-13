#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include "dshlib.h"

// Store the last return code for use with 'rc' command
static int last_return_code = 0;

// Function to trim leading and trailing whitespace
void trim_whitespace(char *str) {
    if (!str) return;
    
    char *start = str;
    while(isspace((unsigned char)*start)) start++;
    
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    
    char *end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    *(end + 1) = '\0';
}

// Parse a command into a cmd_buff structure
int build_cmd_buff(char *cmd_line, cmd_buff_t *cmd_buff) {
    if (!cmd_line || !cmd_buff) return ERR_MEMORY;
    
    clear_cmd_buff(cmd_buff);
    trim_whitespace(cmd_line);
    
    if (strlen(cmd_line) == 0) {
        return WARN_NO_CMDS;
    }
    
    int arg_count = 0;
    char *str = cmd_line;
    bool in_quotes = false;
    char *token_start = str;
    char *curr_token;
    
    // Process command and arguments
    while (*str && arg_count < CMD_ARGV_MAX - 1) {
        if (*str == '"') {
            if (!in_quotes) {
                // Start of quoted section
                if (str > token_start) {
                    // There's a token before the quote
                    *str = '\0';
                    curr_token = strdup(token_start);
                    if (curr_token) {
                        cmd_buff->argv[arg_count++] = curr_token;
                    }
                }
                token_start = str + 1;
                in_quotes = true;
            } else {
                // End of quoted section
                *str = '\0';
                curr_token = strdup(token_start);
                if (curr_token) {
                    cmd_buff->argv[arg_count++] = curr_token;
                }
                token_start = str + 1;
                in_quotes = false;
            }
        } else if (!in_quotes && isspace(*str)) {
            if (str > token_start) {
                // End of unquoted token
                *str = '\0';
                curr_token = strdup(token_start);
                if (curr_token) {
                    cmd_buff->argv[arg_count++] = curr_token;
                }
            }
            token_start = str + 1;
        }
        str++;
    }
    
    // Handle last token
    if (*token_start && arg_count < CMD_ARGV_MAX - 1) {
        if (in_quotes) {
            // Unclosed quote - treat rest as quoted
            curr_token = strdup(token_start);
        } else {
            // Normal token
            curr_token = strdup(token_start);
        }
        
        if (curr_token) {
            cmd_buff->argv[arg_count++] = curr_token;
        }
    }
    
    // Set last argument to NULL
    cmd_buff->argv[arg_count] = NULL;
    cmd_buff->argc = arg_count;
    
    return OK;
}

// Parse a command line into a list of commands, splitting by pipe
int build_cmd_list(char *cmd_line, command_list_t *clist) {
    if (!cmd_line || !clist) return ERR_MEMORY;
    
    // Initialize command list
    memset(clist, 0, sizeof(command_list_t));
    
    trim_whitespace(cmd_line);
    if (strlen(cmd_line) == 0) {
        return WARN_NO_CMDS;
    }
    
    // Split command by pipes
    char *token;
    char *rest = cmd_line;
    int cmd_count = 0;
    
    while ((token = strtok_r(rest, PIPE_STRING, &rest)) && cmd_count < CMD_MAX) {
        // Parse each individual command
        int rc = build_cmd_buff(token, &clist->commands[cmd_count]);
        if (rc != OK && rc != WARN_NO_CMDS) {
            // Error parsing command
            return rc;
        }
        
        // Only count non-empty commands
        if (rc != WARN_NO_CMDS) {
            cmd_count++;
        }
    }
    
    if (token != NULL) {
        return ERR_TOO_MANY_COMMANDS;
    }
    
    clist->num = cmd_count;
    
    return (cmd_count > 0) ? OK : WARN_NO_CMDS;
}

// Free resources in a command list
int free_cmd_list(command_list_t *cmd_lst) {
    if (!cmd_lst) return ERR_MEMORY;
    
    for (int i = 0; i < cmd_lst->num; i++) {
        clear_cmd_buff(&cmd_lst->commands[i]);
    }
    
    cmd_lst->num = 0;
    return OK;
}

// Execute the cd built-in command
int exec_cd_command(cmd_buff_t *cmd) {
    if (!cmd || cmd->argc < 1) return ERR_CMD_ARGS_BAD;
    
    // If no arguments, do nothing
    if (cmd->argc == 1) {
        return OK;
    }
    
    // Change directory
    if (chdir(cmd->argv[1]) != 0) {
        fprintf(stderr, "cd: %s\n", strerror(errno));
        last_return_code = errno;
        return ERR_EXEC_CMD;
    }
    
    last_return_code = 0;
    return OK;
}

// Execute the rc built-in command
int exec_rc_command(cmd_buff_t *cmd __attribute__((unused))) {
    printf("%d\n", last_return_code);
    return OK;
}

// Execute a single external command
int exec_external_command(cmd_buff_t *cmd) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        last_return_code = errno;
        return ERR_EXEC_CMD;
    }
    
    if (pid == 0) {
        // Child process
        
        // Handle input/output redirection if supported
        if (cmd->input_file) {
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd < 0) {
                perror("open input file");
                exit(errno);
            }
            if (dup2(fd, STDIN_FILENO) < 0) {
                perror("dup2 stdin");
                exit(errno);
            }
            close(fd);
        }
        
        if (cmd->output_file) {
            int flags = O_WRONLY | O_CREAT;
            if (cmd->append_mode) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }
            
            int fd = open(cmd->output_file, flags, 0644);
            if (fd < 0) {
                perror("open output file");
                exit(errno);
            }
            if (dup2(fd, STDOUT_FILENO) < 0) {
                perror("dup2 stdout");
                exit(errno);
            }
            close(fd);
        }
        
        // Execute the command
        execvp(cmd->argv[0], cmd->argv);
        
        // If execvp returns, there was an error
        if (errno == ENOENT) {
            fprintf(stderr, "Command not found in PATH\n");
        } else if (errno == EACCES) {
            fprintf(stderr, "Permission denied\n");
        } else {
            fprintf(stderr, "Error executing command\n");
        }
        
        exit(errno);
    }
    
    // Parent process
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        last_return_code = WEXITSTATUS(status);
    } else {
        last_return_code = ERR_EXEC_CMD;
    }
    
    return OK;
}

// Execute a pipeline of commands
int execute_pipeline(command_list_t *clist) {
    if (!clist || clist->num == 0) return ERR_MEMORY;
    
    // If only one command, handle it directly
    if (clist->num == 1) {
        Built_In_Cmds cmd_type = match_command(clist->commands[0].argv[0]);
        
        if (cmd_type == BI_CMD_EXIT) {
            return OK_EXIT;
        } else if (cmd_type == BI_CMD_CD) {
            return exec_cd_command(&clist->commands[0]);
        } else if (cmd_type == BI_CMD_RC) {
            return exec_rc_command(&clist->commands[0]);
        } else {
            return exec_external_command(&clist->commands[0]);
        }
    }
    
    // Setup pipes for commands
    int pipes[CMD_MAX-1][2];
    pid_t child_pids[CMD_MAX];
    
    // Create all pipes needed
    for (int i = 0; i < clist->num - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return ERR_EXEC_CMD;
        }
    }
    
    // Create all child processes
    for (int i = 0; i < clist->num; i++) {
        child_pids[i] = fork();
        
        if (child_pids[i] < 0) {
            perror("fork");
            
            // Close all pipes
            for (int j = 0; j < clist->num - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Kill any child processes already created
            for (int j = 0; j < i; j++) {
                kill(child_pids[j], SIGTERM);
                waitpid(child_pids[j], NULL, 0);
            }
            
            return ERR_EXEC_CMD;
        }
        
        if (child_pids[i] == 0) {
            // Child process
            
            // Handle stdin from previous pipe (except first command)
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            } else if (clist->commands[i].input_file) {
                // First command with input redirection
                int fd = open(clist->commands[i].input_file, O_RDONLY);
                if (fd < 0) {
                    perror("open input file");
                    exit(errno);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            
            // Handle stdout to next pipe (except last command)
            if (i < clist->num - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            } else if (clist->commands[i].output_file) {
                // Last command with output redirection
                int flags = O_WRONLY | O_CREAT;
                if (clist->commands[i].append_mode) {
                    flags |= O_APPEND;
                } else {
                    flags |= O_TRUNC;
                }
                
                int fd = open(clist->commands[i].output_file, flags, 0644);
                if (fd < 0) {
                    perror("open output file");
                    exit(errno);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            
            // Close all pipe file descriptors
            for (int j = 0; j < clist->num - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Execute the command
            execvp(clist->commands[i].argv[0], clist->commands[i].argv);
            
            // If execvp returns, there was an error
            fprintf(stderr, "execvp failed for %s: %s\n", 
                    clist->commands[i].argv[0], strerror(errno));
            exit(errno);
        }
    }
    
    // Parent process: close all pipe file descriptors
    for (int i = 0; i < clist->num - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Wait for all child processes to complete
    int status;
    for (int i = 0; i < clist->num; i++) {
        waitpid(child_pids[i], &status, 0);
        if (i == clist->num - 1 && WIFEXITED(status)) {
            last_return_code = WEXITSTATUS(status);
        }
    }
    
    return OK;
}

// Match a command to see if it's a built-in
Built_In_Cmds match_command(const char *input) {
    if (!input) return BI_NOT_BI;
    
    if (strcmp(input, EXIT_CMD) == 0) return BI_CMD_EXIT;
    if (strcmp(input, "cd") == 0) return BI_CMD_CD;
    if (strcmp(input, "rc") == 0) return BI_CMD_RC;
    if (strcmp(input, "dragon") == 0) return BI_CMD_DRAGON;
    if (strcmp(input, "stop-server") == 0) return BI_CMD_STOP_SVR;
    
    return BI_NOT_BI;
}

// Main command loop implementation
int exec_local_cmd_loop() {
    command_list_t cmd_list;
    char input_buffer[ARG_MAX];
    int rc;
    
    printf("%s", SH_PROMPT);  // Initial prompt
    
    while (1) {
        if (fgets(input_buffer, ARG_MAX, stdin) == NULL) {
            printf("\n");
            return OK;
        }
        
        input_buffer[strcspn(input_buffer, "\n")] = '\0';
        
        // Parse the command line into a command list
        rc = build_cmd_list(input_buffer, &cmd_list);
        
        if (rc == WARN_NO_CMDS) {
            printf("%s", CMD_WARN_NO_CMD);
        } else if (rc == ERR_TOO_MANY_COMMANDS) {
            printf(CMD_ERR_PIPE_LIMIT, CMD_MAX);
        } else if (rc != OK) {
            fprintf(stderr, "Error parsing command\n");
        } else {
            // Execute the pipeline
            rc = execute_pipeline(&cmd_list);
            
            if (rc == OK_EXIT) {
                free_cmd_list(&cmd_list);
                return OK_EXIT;
            }
        }
        
        printf("%s", SH_PROMPT);
        free_cmd_list(&cmd_list);
    }
    
    return OK;
}

// Allocate and initialize a command buffer
int alloc_cmd_buff(cmd_buff_t *cmd_buff) {
    if (!cmd_buff) return ERR_MEMORY;
    
    cmd_buff->argc = 0;
    cmd_buff->_cmd_buffer = NULL;
    cmd_buff->input_file = NULL;
    cmd_buff->output_file = NULL;
    cmd_buff->append_mode = false;
    memset(cmd_buff->argv, 0, sizeof(char*) * CMD_ARGV_MAX);
    
    return OK;
}

int free_cmd_buff(cmd_buff_t *cmd_buff) {
    return clear_cmd_buff(cmd_buff);
}

int clear_cmd_buff(cmd_buff_t *cmd_buff) {
    if (!cmd_buff) return ERR_MEMORY;
    
    // Free arguments
    for (int i = 0; i < cmd_buff->argc; i++) {
        if (cmd_buff->argv[i]) {
            free(cmd_buff->argv[i]);
            cmd_buff->argv[i] = NULL;
        }
    }
    
    // Free redirection files
    if (cmd_buff->input_file) {
        free(cmd_buff->input_file);
        cmd_buff->input_file = NULL;
    }
    
    if (cmd_buff->output_file) {
        free(cmd_buff->output_file);
        cmd_buff->output_file = NULL;
    }
    
    // Free any other allocated memory
    if (cmd_buff->_cmd_buffer) {
        free(cmd_buff->_cmd_buffer);
        cmd_buff->_cmd_buffer = NULL;
    }
    
    cmd_buff->argc = 0;
    cmd_buff->append_mode = false;
    
    return OK;
}