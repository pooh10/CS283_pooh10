#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>

//INCLUDES for extra credit
//#include <signal.h>
//#include <pthread.h>
//-------------------------

#include "dshlib.h"
#include "rshlib.h"

static int threaded_mode = 0;

/*
 * start_server(ifaces, port, is_threaded)
 */
int start_server(char *ifaces, int port, int is_threaded){
    int svr_socket;
    int rc;

    threaded_mode = is_threaded;

    svr_socket = boot_server(ifaces, port);
    if (svr_socket < 0){
        int err_code = svr_socket;  // server socket will carry error code
        fprintf(stderr, "Failed to boot server: %d\n", err_code);
        return err_code;
    }

    printf("Server started successfully on %s:%d\n", ifaces, port);
    rc = process_cli_requests(svr_socket);

    printf("Stopping server...\n");
    stop_server(svr_socket);

    return rc;
}

/*
 * stop_server(svr_socket)
 */
int stop_server(int svr_socket){
    printf("Closing server socket %d\n", svr_socket);
    return close(svr_socket);
}

/*
 * boot_server(ifaces, port)
 */
int boot_server(char *ifaces, int port){
    int svr_socket;
    int ret;
    
    struct sockaddr_in addr;

    /* Create local socket. */
    svr_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (svr_socket == -1) {
        perror("socket");
        return ERR_RDSH_COMMUNICATION;
    }

    /*
     * Set socket option to reuse address
     */
    int enable = 1;
    setsockopt(svr_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    
    /* Bind socket to socket name. */
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ifaces);
    addr.sin_port = htons(port);

    ret = bind(svr_socket, (const struct sockaddr *) &addr,
               sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("bind");
        close(svr_socket);
        return ERR_RDSH_COMMUNICATION;
    }

    /*
     * Prepare for accepting connections. The backlog size is set
     * to 20. So while one request is being processed other requests
     * can be waiting.
     */
    ret = listen(svr_socket, 20);
    if (ret == -1) {
        perror("listen");
        close(svr_socket);
        return ERR_RDSH_COMMUNICATION;
    }

    return svr_socket;
}

/*
 * process_cli_requests(svr_socket)
 */
int process_cli_requests(int svr_socket){
    int cli_socket;
    int rc = OK;    
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while(1){
        // Accept client connection
        cli_socket = accept(svr_socket, (struct sockaddr*)&client_addr, &client_len);
        if (cli_socket < 0) {
            perror("accept");
            return ERR_RDSH_COMMUNICATION;
        }
        
        printf("Client connected from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        // Handle client requests
        rc = exec_client_requests(cli_socket);
        
        // Check if we need to stop the server
        if (rc == OK_EXIT) {
            printf("%s", RCMD_MSG_SVR_STOP_REQ);
            break;
        }
        
        printf("%s", RCMD_MSG_CLIENT_EXITED);
    }

    return rc;
}

/*
 * send_message_eof(cli_socket)
 */
int send_message_eof(int cli_socket){
    int send_len = 1;  // Just sending 1 byte - the EOF char
    int sent_len;
    
    sent_len = send(cli_socket, &RDSH_EOF_CHAR, send_len, 0);

    if (sent_len != send_len){
        perror("send EOF failed");
        return ERR_RDSH_COMMUNICATION;
    }
    return OK;
}

/*
 * send_message_string(cli_socket, char *buff)
 */
int send_message_string(int cli_socket, char *buff){
    int send_len = strlen(buff);
    int sent_len;
    
    // Send the message
    sent_len = send(cli_socket, buff, send_len, 0);
    if (sent_len != send_len) {
        fprintf(stderr, CMD_ERR_RDSH_SEND, sent_len, send_len);
        return ERR_RDSH_COMMUNICATION;
    }
    
    return OK;
}

/*
 * exec_client_requests(cli_socket)
 */
int exec_client_requests(int cli_socket) {
    int io_size;
    command_list_t cmd_list;
    int rc = OK;
    char *io_buff;
    
    // Allocate buffer for client commands
    io_buff = malloc(RDSH_COMM_BUFF_SZ);
    if (io_buff == NULL){
        fprintf(stderr, "Failed to allocate buffer\n");
        close(cli_socket);
        return ERR_RDSH_SERVER;
    }
    
    // Process client commands
    while(1) {
        // Clear buffer and reset command list
        memset(io_buff, 0, RDSH_COMM_BUFF_SZ);
        memset(&cmd_list, 0, sizeof(command_list_t));
        
        // Receive command from client
        io_size = recv(cli_socket, io_buff, RDSH_COMM_BUFF_SZ - 1, 0);
        
        if (io_size <= 0) {
            if (io_size < 0) {
                perror("recv");
            }
            // Either error or client disconnected
            fprintf(stderr, "Client disconnected or recv error\n");
            rc = ERR_RDSH_COMMUNICATION;
            break;
        }
        
        // Ensure null termination
        io_buff[io_size] = '\0';
        printf("Received command: %s\n", io_buff);
        
        // Check for empty command
        if (strlen(io_buff) == 0) {
            send_message_string(cli_socket, CMD_WARN_NO_CMD);
            send_message_eof(cli_socket);
            continue;
        }
        
        // Check for special commands
        if (strcmp(io_buff, EXIT_CMD) == 0) {
            rc = OK;
            break;
        }
        
        if (strcmp(io_buff, "stop-server") == 0) {
            send_message_string(cli_socket, "Server shutting down\n");
            send_message_eof(cli_socket);
            rc = OK_EXIT;
            break;
        }
        
        // Parse command into command list
        rc = build_cmd_list(io_buff, &cmd_list);
        
        if (rc == WARN_NO_CMDS) {
            send_message_string(cli_socket, CMD_WARN_NO_CMD);
            send_message_eof(cli_socket);
            continue;
        } else if (rc == ERR_TOO_MANY_COMMANDS) {
            char err_msg[100];
            sprintf(err_msg, CMD_ERR_PIPE_LIMIT, CMD_MAX);
            send_message_string(cli_socket, err_msg);
            send_message_eof(cli_socket);
            continue;
        } else if (rc != OK) {
            send_message_string(cli_socket, CMD_ERR_RDSH_EXEC);
            send_message_eof(cli_socket);
            continue;
        }
        
        // Execute the commands
        rc = rsh_execute_pipeline(cli_socket, &cmd_list);
        
        // Send EOF to indicate end of command execution
        rc = send_message_eof(cli_socket);
        if (rc != OK) {
            fprintf(stderr, "Failed to send EOF\n");
            break;
        }
        
        free_cmd_list(&cmd_list);
    }
    
    free(io_buff);
    close(cli_socket);
    return rc;
}

/*
 * rsh_execute_pipeline(int cli_sock, command_list_t *clist)
 */
int rsh_execute_pipeline(int cli_sock, command_list_t *clist) {
    if (!clist || clist->num == 0) {
        return ERR_RDSH_CMD_EXEC;
    }
    
    // Handle built-in commands directly
    Built_In_Cmds bi_cmd = rsh_match_command(clist->commands[0].argv[0]);
    
    if (bi_cmd == BI_CMD_EXIT) {
        return OK_EXIT;
    } else if (bi_cmd == BI_CMD_STOP_SVR) {
        return STOP_SERVER_SC;
    } else if (bi_cmd == BI_CMD_CD && clist->num == 1) {
        if (clist->commands[0].argc > 1) {
            if (chdir(clist->commands[0].argv[1]) != 0) {
                char err_msg[256];
                sprintf(err_msg, "cd: %s: %s\n", clist->commands[0].argv[1], strerror(errno));
                send_message_string(cli_sock, err_msg);
                return ERR_RDSH_CMD_EXEC;
            }
        }
        return OK;
    }

    int pipes[CMD_MAX-1][2];
    pid_t pids[CMD_MAX];
    int status;
    int exit_code = 0;
    
    // Create all necessary pipes
    for (int i = 0; i < clist->num - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return ERR_RDSH_CMD_EXEC;
        }
    }
    
    // Fork and execute each command in the pipeline
    for (int i = 0; i < clist->num; i++) {
        pids[i] = fork();
        
        if (pids[i] < 0) {
            perror("fork");
            
            // Close all pipes
            for (int j = 0; j < clist->num - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            return ERR_RDSH_CMD_EXEC;
        }
        
        if (pids[i] == 0) {
            // Child process
            
            // Set up stdin (from previous pipe or socket for first command)
            if (i == 0) {
                // First command gets input from client socket
                dup2(cli_sock, STDIN_FILENO);
            } else {
                // Other commands get input from previous pipe
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            
            // Set up stdout (to next pipe or socket for last command)
            if (i == clist->num - 1) {
                // Last command sends output to client socket
                dup2(cli_sock, STDOUT_FILENO);
                dup2(cli_sock, STDERR_FILENO);
            } else {
                // Other commands send output to next pipe
                dup2(pipes[i][1], STDOUT_FILENO);
                // Error output goes to client socket for all commands
                dup2(cli_sock, STDERR_FILENO);
            }
            
            // Close all pipe ends in child
            for (int j = 0; j < clist->num - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Execute the command
            execvp(clist->commands[i].argv[0], clist->commands[i].argv);
            
            // If execvp returns, there was an error
            fprintf(stderr, "execvp failed: %s\n", strerror(errno));
            char error_msg[256];
            sprintf(error_msg, "Command not found: %s\n", clist->commands[i].argv[0]);
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            exit(errno);
        }
    }
    
    // Parent process: close all pipe ends
    for (int i = 0; i < clist->num - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Wait for all children to complete
    for (int i = 0; i < clist->num; i++) {
        if (waitpid(pids[i], &status, 0) > 0) {
            if (WIFEXITED(status)) {
                if (i == clist->num - 1) {
                    // Return exit code of last command
                    exit_code = WEXITSTATUS(status);
                }
            }
        }
    }
    
    return exit_code;
}

/*
 * rsh_match_command(const char *input)
 */
Built_In_Cmds rsh_match_command(const char *input)
{
    if (!input) return BI_NOT_BI;
    
    if (strcmp(input, EXIT_CMD) == 0) return BI_CMD_EXIT;
    if (strcmp(input, "dragon") == 0) return BI_CMD_DRAGON;
    if (strcmp(input, "cd") == 0) return BI_CMD_CD;
    if (strcmp(input, "stop-server") == 0) return BI_CMD_STOP_SVR;
    if (strcmp(input, "rc") == 0) return BI_CMD_RC;
    
    return BI_NOT_BI;
}

/*
 * rsh_built_in_cmd(cmd_buff_t *cmd)
 */
Built_In_Cmds rsh_built_in_cmd(cmd_buff_t *cmd)
{
    if (!cmd || cmd->argc < 1) return BI_NOT_BI;
    
    Built_In_Cmds ctype = rsh_match_command(cmd->argv[0]);

    switch (ctype)
    {
    case BI_CMD_EXIT:
        return BI_CMD_EXIT;
    case BI_CMD_STOP_SVR:
        return BI_CMD_STOP_SVR;
    case BI_CMD_RC:
        return BI_CMD_RC;
    case BI_CMD_CD:
        if (cmd->argc > 1) {
            if (chdir(cmd->argv[1]) != 0) {
                perror("cd");
            }
        }
        return BI_EXECUTED;
    case BI_CMD_DRAGON:
        return BI_EXECUTED;
    default:
        return BI_NOT_BI;
    }
}