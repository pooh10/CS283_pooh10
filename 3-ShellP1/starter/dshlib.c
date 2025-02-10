#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "dshlib.h"

int build_cmd_list(char *cmd_line, command_list_t *clist) {
    if (!cmd_line || !clist) {
        return ERR_CMD_OR_ARGS_TOO_BIG;
    }

    memset(clist, 0, sizeof(command_list_t));

    // Handle empty command
    char *trimmed = cmd_line;
    while (*trimmed && isspace(*trimmed)) trimmed++;
    if (!*trimmed) {
        return WARN_NO_CMDS;
    }

    // Split by pipe
    char *saveptr;
    char *token = strtok_r(cmd_line, PIPE_STRING, &saveptr);
    
    while (token) {
        if (clist->num >= CMD_MAX) {
            return ERR_TOO_MANY_COMMANDS;
        }

        // Trim leading/trailing spaces
        while (*token && isspace(*token)) token++;
        char *end = token + strlen(token) - 1;
        while (end > token && isspace(*end)) end--;
        *(end + 1) = '\0';

        // Split command and args
        char *cmd = token;
        char *args = strchr(token, SPACE_CHAR);
        
        if (args) {
            *args = '\0';
            args++;
            while (*args && isspace(*args)) args++;
        }

        // Check sizes
        if (strlen(cmd) >= EXE_MAX || (args && strlen(args) >= ARG_MAX)) {
            return ERR_CMD_OR_ARGS_TOO_BIG;
        }

        // Store command and args
        strcpy(clist->commands[clist->num].exe, cmd);
        if (args && *args) {
            strcpy(clist->commands[clist->num].args, args);
        }
        clist->num++;

        token = strtok_r(NULL, PIPE_STRING, &saveptr);
    }

    return OK;
}