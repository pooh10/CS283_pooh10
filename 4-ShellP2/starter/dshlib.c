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

static int last_return_code = 0;

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
    
    while (*str && arg_count < CMD_ARGV_MAX - 1) {
        if (*str == '"') {
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
    
    // Handle last token if not in quotes
    if (*token_start && !in_quotes && arg_count < CMD_ARGV_MAX - 1) {
        curr_token = strdup(token_start);
        if (curr_token) {
            cmd_buff->argv[arg_count++] = curr_token;
        }
    }
    
    cmd_buff->argv[arg_count] = NULL;
    cmd_buff->argc = arg_count;
    
    return OK;
}

int exec_cd_command(cmd_buff_t *cmd) {
    if (!cmd || cmd->argc < 1) return ERR_CMD_ARGS_BAD;
    
    if (cmd->argc == 1) {
        last_return_code = 0;
        return OK;
    }
    
    if (chdir(cmd->argv[1]) != 0) {
        fprintf(stderr, "cd: %s\n", strerror(errno));
        last_return_code = errno;
        return ERR_EXEC_CMD;
    }
    
    last_return_code = 0;
    return OK;
}

int exec_rc_command(cmd_buff_t *cmd __attribute__((unused))) {
    printf("%d", last_return_code);
    return OK;
}

int exec_external_command(cmd_buff_t *cmd) {
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("fork");
        last_return_code = errno;
        return ERR_EXEC_CMD;
    } 
    
    if (pid == 0) {
        // Child process
        execvp(cmd->argv[0], cmd->argv);
        // If execvp returns, there was an error
        if (errno == ENOENT) {
            fprintf(stderr, "%s", CMD_ERR_NOT_FOUND);
            exit(errno);
        } else if (errno == EACCES) {
            fprintf(stderr, "%s", CMD_ERR_PERMISSION);
            exit(errno);
        } else {
            fprintf(stderr, "%s", CMD_ERR_EXECUTE);
            exit(errno);
        }
    }
    
    // Parent process
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        last_return_code = WEXITSTATUS(status);
        return last_return_code;
    }
    
    last_return_code = ERR_EXEC_CMD;
    return ERR_EXEC_CMD;
}

Built_In_Cmds match_command(const char *input) {
    if (!input) return BI_NOT_BI;
    
    if (strcmp(input, EXIT_CMD) == 0) return BI_CMD_EXIT;
    if (strcmp(input, "cd") == 0) return BI_CMD_CD;
    if (strcmp(input, "rc") == 0) return BI_CMD_RC;
    
    return BI_NOT_BI;
}

int exec_local_cmd_loop() {
    cmd_buff_t cmd;
    char input_buffer[ARG_MAX];
    int rc;
    
    if (alloc_cmd_buff(&cmd) != OK) {
        return ERR_MEMORY;
    }
    
    printf("%s", SH_PROMPT);  // Initial prompt
    
    while (1) {
        if (fgets(input_buffer, ARG_MAX, stdin) == NULL) {
            printf("\n");
            free_cmd_buff(&cmd);
            return OK;
        }
        
        input_buffer[strcspn(input_buffer, "\n")] = '\0';
        
        rc = build_cmd_buff(input_buffer, &cmd);
        
        if (rc == WARN_NO_CMDS) {
            printf("%s", CMD_WARN_NO_CMD);
            printf("%s", SH_PROMPT);
            continue;
        }
        
        if (rc != OK) {
            fprintf(stderr, "Error parsing command\n");
            printf("%s", SH_PROMPT);
            continue;
        }
        
        Built_In_Cmds cmd_type = match_command(cmd.argv[0]);
        
        switch (cmd_type) {
    case BI_CMD_EXIT:
        printf("cmd loop returned 0\n");
        free_cmd_buff(&cmd);
        return OK_EXIT;
    
    case BI_CMD_CD:
        rc = exec_cd_command(&cmd);
        printf("\n");  // Add newline after cd output
        break;
    
    case BI_CMD_RC:
        rc = exec_rc_command(&cmd);
        printf("\n");
        break;
    
    case BI_NOT_BI:
        rc = exec_external_command(&cmd);
        break;
        
    default:
        fprintf(stderr, "Unknown command type\n");
        rc = ERR_EXEC_CMD;
}
        
        printf("%s", SH_PROMPT);
        clear_cmd_buff(&cmd);
    }
    
    free_cmd_buff(&cmd);
    return OK;
}

int alloc_cmd_buff(cmd_buff_t *cmd_buff) {
    if (!cmd_buff) return ERR_MEMORY;
    cmd_buff->argc = 0;
    cmd_buff->_cmd_buffer = NULL;
    memset(cmd_buff->argv, 0, sizeof(char*) * CMD_ARGV_MAX);
    return OK;
}

int free_cmd_buff(cmd_buff_t *cmd_buff) {
    if (!cmd_buff) return ERR_MEMORY;
    for (int i = 0; i < cmd_buff->argc; i++) {
        free(cmd_buff->argv[i]);
    }
    cmd_buff->argc = 0;
    memset(cmd_buff->argv, 0, sizeof(char*) * CMD_ARGV_MAX);
    return OK;
}

int clear_cmd_buff(cmd_buff_t *cmd_buff) {
    return free_cmd_buff(cmd_buff);
}