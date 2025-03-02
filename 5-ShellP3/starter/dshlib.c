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
#include <limits.h>

#define REDIR_NONE 0
#define REDIR_IN   1
#define REDIR_OUT  2
#define REDIR_APPEND 3

// Redirection info struct that will be stored in _cmd_buffer
typedef struct {
    int in_type;         // Type of input redirection (REDIR_NONE or REDIR_IN)
    char *in_file;       // Input file name
    int out_type;        // Type of output redirection (REDIR_NONE, REDIR_OUT, or REDIR_APPEND)
    char *out_file;      // Output file name
} redir_info_t;

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

int build_cmd_buff(char *cmd_line, cmd_buff_t *cmd_buff) {
    if (!cmd_line || !cmd_buff) return ERR_MEMORY;
    
    clear_cmd_buff(cmd_buff);
    trim_whitespace(cmd_line);
    
    if (strlen(cmd_line) == 0) {
        return WARN_NO_CMDS;
    }
    
    // Create redirection info structure
    redir_info_t *redir = malloc(sizeof(redir_info_t));
    if (!redir) return ERR_MEMORY;
    
    redir->in_type = REDIR_NONE;
    redir->in_file = NULL;
    redir->out_type = REDIR_NONE;
    redir->out_file = NULL;
    
    // Store redirection info in _cmd_buffer field
    cmd_buff->_cmd_buffer = (char*)redir;
    
    int arg_count = 0;
    char *str = cmd_line;
    bool in_quotes = false;
    char *token_start = str;
    char *curr_token;
    
    // First token is always the command
    char *space = strchr(str, ' ');
    if (space) {
        *space = '\0';
        cmd_buff->argv[arg_count++] = strdup(str);
        str = space + 1;
        while (*str && isspace(*str)) str++;
    } else {
        cmd_buff->argv[arg_count++] = strdup(str);
        str += strlen(str);
    }
    
    token_start = str;
    
    // Process the rest of the tokens, looking for redirection operators
    while (*str && arg_count < CMD_ARGV_MAX - 1) {
        // Check for redirection operators when not in quotes
        if (!in_quotes && (*str == '<' || *str == '>')) {
            // Handle redirection operators
            if (str > token_start) {
                // There's a token before the redirection operator
                *str = '\0';
                curr_token = strdup(token_start);
                if (curr_token) {
                    cmd_buff->argv[arg_count++] = curr_token;
                }
            }
            
            // Determine redirection type
            if (*str == '<') {
                redir->in_type = REDIR_IN;
                *str = '\0';
                str++;
            } else if (*str == '>' && *(str+1) == '>') {
                redir->out_type = REDIR_APPEND;
                *str = '\0';
                str += 2;
            } else { // simple >
                redir->out_type = REDIR_OUT;
                *str = '\0';
                str++;
            }
            
            // Skip whitespace
            while (*str && isspace(*str)) str++;
            
            // The next token is the filename
            token_start = str;
            
            // Find end of filename (next space or end of string)
            while (*str && !isspace(*str) && *str != '<' && *str != '>') str++;
            
            if (str > token_start) {
                char save = *str;
                *str = '\0';
                
                // Store filename in appropriate redirection field
                if (redir->in_type == REDIR_IN && !redir->in_file) {
                    redir->in_file = strdup(token_start);
                } else if ((redir->out_type == REDIR_OUT || redir->out_type == REDIR_APPEND) && !redir->out_file) {
                    redir->out_file = strdup(token_start);
                }
                
                if (save != '\0') {
                    *str = save;
                    // Skip whitespace for next token
                    while (*str && isspace(*str)) str++;
                    token_start = str;
                    continue;
                } else {
                    break; // End of string
                }
            }
        } else if (*str == '"') {
            if (!in_quotes) {
                token_start = str + 1;
                in_quotes = true;
            } else {
                *str = '\0';
                curr_token = strdup(token_start);
                if (curr_token) {
                    cmd_buff->argv[arg_count++] = curr_token;
                }
                in_quotes = false;
                token_start = str + 1;
            }
        } else if (!in_quotes && isspace(*str)) {
            if (str > token_start) {
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
    
    // Handle last token if not in quotes and not a redirection file
    if (*token_start && !in_quotes && arg_count < CMD_ARGV_MAX - 1 && 
        redir->in_file != token_start && redir->out_file != token_start) {
        curr_token = strdup(token_start);
        if (curr_token) {
            cmd_buff->argv[arg_count++] = curr_token;
        }
    }
    
    cmd_buff->argv[arg_count] = NULL;
    cmd_buff->argc = arg_count;
    
    return OK;
}

// Parse a pipe-separated command line into a command list
int build_cmd_list(char *cmd_line, command_list_t *clist) {
    if (!cmd_line || !clist) return ERR_MEMORY;
    
    // Initialize command list
    clist->num = 0;
    
    trim_whitespace(cmd_line);
    if (strlen(cmd_line) == 0) {
        return WARN_NO_CMDS;
    }
    
    // Make a copy of cmd_line since strtok modifies the string
    char *cmd_copy = strdup(cmd_line);
    if (!cmd_copy) return ERR_MEMORY;
    
    // Split by pipe character
    char *token = strtok(cmd_copy, PIPE_STRING);
    int cmd_count = 0;
    
    // Process each command separated by pipes
    while (token != NULL && cmd_count < CMD_MAX) {
        // Parse the command
        int rc = build_cmd_buff(token, &clist->commands[cmd_count]);
        if (rc != OK && rc != WARN_NO_CMDS) {
            free(cmd_copy);
            return rc;
        }
        
        // Skip empty commands
        if (rc != WARN_NO_CMDS) {
            cmd_count++;
        }
        
        token = strtok(NULL, PIPE_STRING);
    }
    
    // Check if we ran out of command slots
    if (token != NULL && cmd_count >= CMD_MAX) {
        free(cmd_copy);
        return ERR_TOO_MANY_COMMANDS;
    }
    
    clist->num = cmd_count;
    free(cmd_copy);
    
    return cmd_count > 0 ? OK : WARN_NO_CMDS;
}

// Free all resources in a command list
int free_cmd_list(command_list_t *cmd_lst) {
    if (!cmd_lst) return ERR_MEMORY;
    
    for (int i = 0; i < cmd_lst->num; i++) {
        clear_cmd_buff(&cmd_lst->commands[i]);
    }
    
    cmd_lst->num = 0;
    return OK;
}

// Execute the built-in CD command
int exec_cd_command(cmd_buff_t *cmd) {
    if (!cmd || cmd->argc < 1) return ERR_CMD_ARGS_BAD;
    
    // If no arguments, do nothing (per assignment spec)
    if (cmd->argc == 1) {
        return OK;
    }
    
    // Change directory
    if (chdir(cmd->argv[1]) != 0) {
        fprintf(stderr, "cd: %s\n", strerror(errno));
        return ERR_EXEC_CMD;
    }
    
    return OK;
}

// Execute a single external command
int exec_external_command(cmd_buff_t *cmd) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        return ERR_EXEC_CMD;
    } 
    
    if (pid == 0) {
        // Child process
        
        // Handle redirection if present
        if (cmd->_cmd_buffer) {
            redir_info_t *redir = (redir_info_t*)cmd->_cmd_buffer;
            
            // Input redirection
            if (redir->in_type == REDIR_IN && redir->in_file) {
                int fd = open(redir->in_file, O_RDONLY);
                if (fd == -1) {
                    fprintf(stderr, "Error opening input file: %s\n", strerror(errno));
                    exit(errno);
                }
                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("dup2 stdin");
                    exit(EXIT_FAILURE);
                }
                close(fd);
            }
            
            // Output redirection
            if (redir->out_type != REDIR_NONE && redir->out_file) {
                int flags;
                if (redir->out_type == REDIR_APPEND) {
                    flags = O_WRONLY | O_CREAT | O_APPEND;
                } else { // REDIR_OUT
                    flags = O_WRONLY | O_CREAT | O_TRUNC;
                }
                
                int fd = open(redir->out_file, flags, 0644);
                if (fd == -1) {
                    fprintf(stderr, "Error opening output file: %s\n", strerror(errno));
                    exit(errno);
                }
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("dup2 stdout");
                    exit(EXIT_FAILURE);
                }
                close(fd);
            }
        }
        
        execvp(cmd->argv[0], cmd->argv);
        
        // If execvp returns, there was an error
        if (errno == ENOENT) {
            fprintf(stderr, "Command not found in PATH\n");
        } else if (errno == EACCES) {
            fprintf(stderr, "Permission denied\n");
        } else {
            fprintf(stderr, "error executing command\n");
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
            printf("exiting...\n");
            return OK_EXIT;
        } else if (cmd_type == BI_CMD_CD) {
            return exec_cd_command(&clist->commands[0]);
        } else {
            return exec_external_command(&clist->commands[0]);
        }
    }
    
    // Setup for piped commands
    int pipe_fds[CMD_MAX-1][2]; // Array of pipe file descriptors
    pid_t child_pids[CMD_MAX];  // Array to store child PIDs
    
    // Create all pipes needed
    for (int i = 0; i < clist->num - 1; i++) {
        if (pipe(pipe_fds[i]) == -1) {
            perror("pipe");
            // Close any pipes already created
            for (int j = 0; j < i; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
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
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
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
    
    // Handle redirection for this command
    redir_info_t *redir = NULL;
    if (clist->commands[i]._cmd_buffer) {
        redir = (redir_info_t*)clist->commands[i]._cmd_buffer;
    }
    
    // Input redirection: Either from previous pipe or from file
    if (i > 0) {
        // From previous pipe takes precedence
        if (dup2(pipe_fds[i-1][0], STDIN_FILENO) == -1) {
            perror("dup2 stdin");
            exit(EXIT_FAILURE);
        }
    } else if (redir && redir->in_type == REDIR_IN && redir->in_file) {
        // From file (only for the first command in pipeline)
        int fd = open(redir->in_file, O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "Error opening input file: %s\n", strerror(errno));
            exit(errno);
        }
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("dup2 stdin");
            exit(EXIT_FAILURE);
        }
        close(fd);
    }
    
    // Output redirection: Either to next pipe or to file
    if (i < clist->num - 1) {
        // To next pipe takes precedence
        if (dup2(pipe_fds[i][1], STDOUT_FILENO) == -1) {
            perror("dup2 stdout");
            exit(EXIT_FAILURE);
        }
    } else if (redir && redir->out_type != REDIR_NONE && redir->out_file) {
        // To file (only for the last command in pipeline)
        int flags;
        if (redir->out_type == REDIR_APPEND) {
            flags = O_WRONLY | O_CREAT | O_APPEND;
        } else { // REDIR_OUT
            flags = O_WRONLY | O_CREAT | O_TRUNC;
        }
        
        int fd = open(redir->out_file, flags, 0644);
        if (fd == -1) {
            fprintf(stderr, "Error opening output file: %s\n", strerror(errno));
            exit(errno);
        }
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2 stdout");
            exit(EXIT_FAILURE);
        }
        close(fd);
    }
            
            // Close all pipe file descriptors
            for (int j = 0; j < clist->num - 1; j++) {
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            
            // Execute the command
            execvp(clist->commands[i].argv[0], clist->commands[i].argv);
            
            // If we get here, execvp failed
            if (errno == ENOENT) {
                fprintf(stderr, "Command not found in PATH\n");
            } else if (errno == EACCES) {
                fprintf(stderr, "Permission denied\n");
            } else {
                fprintf(stderr, "error executing command\n");
            }
            exit(errno);
        }
    }
    
    // Parent process - close all pipe file descriptors
    for (int i = 0; i < clist->num - 1; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }
    
    // Wait for all child processes to complete
    int status;
    for (int i = 0; i < clist->num; i++) {
        waitpid(child_pids[i], &status, 0);
        if (WIFEXITED(status)) {
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
    
    return BI_NOT_BI;
}

/*
 * Main command loop implementation
 * This function prompts for user input, parses commands,
 * and executes them until exit is requested
 */
int exec_local_cmd_loop() {
    command_list_t cmd_list;
    char input_buffer[ARG_MAX];
    int rc;
    
    printf("%s", SH_PROMPT);  // Initial prompt
    
    while (1) {
        // Read input from user
        if (fgets(input_buffer, ARG_MAX, stdin) == NULL) {
            printf("\n");
            return OK;
        }
        
        // Remove trailing newline
        input_buffer[strcspn(input_buffer, "\n")] = '\0';
        
        // Parse the command line into a command list
        rc = build_cmd_list(input_buffer, &cmd_list);
        
        if (rc == WARN_NO_CMDS) {
            printf("%s", CMD_WARN_NO_CMD);
            printf("%s", SH_PROMPT);
            continue;
        }
        
        if (rc == ERR_TOO_MANY_COMMANDS) {
            printf(CMD_ERR_PIPE_LIMIT, CMD_MAX);
            printf("%s", SH_PROMPT);
            continue;
        }
        
        if (rc != OK) {
            fprintf(stderr, "Error parsing command\n");
            printf("%s", SH_PROMPT);
            continue;
        }
        
        // Execute the pipeline
        rc = execute_pipeline(&cmd_list);
        
        if (rc == OK_EXIT) {
            free_cmd_list(&cmd_list);
            return OK_EXIT;
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
    memset(cmd_buff->argv, 0, sizeof(char*) * CMD_ARGV_MAX);
    
    return OK;
}

int free_cmd_buff(cmd_buff_t *cmd_buff) {
    return clear_cmd_buff(cmd_buff);
}

int clear_cmd_buff(cmd_buff_t *cmd_buff) {
    if (!cmd_buff) return ERR_MEMORY;
    
    // Free redirection info if it exists
    if (cmd_buff->_cmd_buffer) {
        redir_info_t *redir = (redir_info_t*)cmd_buff->_cmd_buffer;
        if (redir->in_file) free(redir->in_file);
        if (redir->out_file) free(redir->out_file);
        free(redir);
        cmd_buff->_cmd_buffer = NULL;
    }
    
    // Free command arguments
    for (int i = 0; i < cmd_buff->argc; i++) {
        if (cmd_buff->argv[i]) {
            free(cmd_buff->argv[i]);
            cmd_buff->argv[i] = NULL;
        }
    }
    
    cmd_buff->argc = 0;
    
    return OK;
}