#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <fcntl.h>

#include "dshlib.h"
#include "rshlib.h"


/*
 * exec_remote_cmd_loop(server_ip, port)
 */
int exec_remote_cmd_loop(char *address, int port)
{
    char *cmd_buff;
    char *rsp_buff;
    int cli_socket;
    ssize_t io_size;
    int is_eof;
    int rc = OK;

    // Allocate buffers for sending and receiving
    cmd_buff = malloc(RDSH_COMM_BUFF_SZ);
    rsp_buff = malloc(RDSH_COMM_BUFF_SZ);
    if (!cmd_buff || !rsp_buff) {
        fprintf(stderr, "Failed to allocate buffers\n");
        return client_cleanup(0, cmd_buff, rsp_buff, ERR_MEMORY);
    }

    // Connect to server
    cli_socket = start_client(address, port);
    if (cli_socket < 0) {
        fprintf(stderr, "Failed to connect to server %s:%d\n", address, port);
        return client_cleanup(cli_socket, cmd_buff, rsp_buff, ERR_RDSH_CLIENT);
    }

    printf("Connected to server %s:%d\n", address, port);

    while (1) 
    {
        printf("%s", SH_PROMPT);
        if (fgets(cmd_buff, RDSH_COMM_BUFF_SZ, stdin) == NULL) {
            printf("\n");
            break;
        }
        
        // Remove trailing newline
        cmd_buff[strcspn(cmd_buff, "\n")] = '\0';
        
        // Skip empty commands
        if (strlen(cmd_buff) == 0) {
            continue;
        }
        
        // Check for local exit command
        if (strcmp(cmd_buff, EXIT_CMD) == 0) {
            // Send exit to server too
            io_size = send(cli_socket, cmd_buff, strlen(cmd_buff) + 1, 0);
            if (io_size < 0) {
                perror("send");
                rc = ERR_RDSH_COMMUNICATION;
                break;
            }
            
            // Wait for server to acknowledge
            io_size = recv(cli_socket, rsp_buff, RDSH_COMM_BUFF_SZ - 1, 0);
            if (io_size > 0) {
                is_eof = (rsp_buff[io_size-1] == RDSH_EOF_CHAR) ? 1 : 0;
                if (is_eof) {
                    rsp_buff[io_size-1] = '\0';
                    printf("%.*s", (int)io_size-1, rsp_buff);
                }
            }
            
            rc = OK;
            break;
        }
        
        // Send command to server (including null terminator)
        io_size = send(cli_socket, cmd_buff, strlen(cmd_buff) + 1, 0);
        if (io_size < 0) {
            perror("send");
            rc = ERR_RDSH_COMMUNICATION;
            break;
        }
        
        while (1) {
            memset(rsp_buff, 0, RDSH_COMM_BUFF_SZ);
            io_size = recv(cli_socket, rsp_buff, RDSH_COMM_BUFF_SZ - 1, 0);
            
            if (io_size <= 0) {
                if (io_size < 0) {
                    perror("recv");
                } else {
                    fprintf(stderr, "%s", RCMD_SERVER_EXITED);
                }
                rc = ERR_RDSH_COMMUNICATION;
                goto cleanup;  // Use goto for this exceptional case
            }
            
            // Check if this is the last chunk (ends with EOF)
            is_eof = (rsp_buff[io_size-1] == RDSH_EOF_CHAR) ? 1 : 0;
            
            // Print response (don't print the EOF character)
            if (is_eof) {
                printf("%.*s", (int)io_size-1, rsp_buff);
            } else {
                printf("%.*s", (int)io_size, rsp_buff);
            }
            
            // If we got the EOF marker, we're done with this command
            if (is_eof) {
                break;
            }
        }
        
        // Check for server stop command
        if (strcmp(cmd_buff, "stop-server") == 0) {
            printf("Server has been stopped\n");
            rc = OK;
            break;
        }
    }

cleanup:
    return client_cleanup(cli_socket, cmd_buff, rsp_buff, rc);
}


/*
 * start_client(server_ip, port)
 */
int start_client(char *server_ip, int port){
    struct sockaddr_in addr;
    int cli_socket;
    int ret;

    // Create socket
    cli_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (cli_socket == -1) {
        perror("socket");
        return ERR_RDSH_CLIENT;
    }

    // Set up server address
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(server_ip);
    addr.sin_port = htons(port);

    // Connect to server
    ret = connect(cli_socket, (const struct sockaddr *) &addr,
                  sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("connect");
        close(cli_socket);
        return ERR_RDSH_CLIENT;
    }

    return cli_socket;
}


/*
 * client_cleanup(int cli_socket, char *cmd_buff, char *rsp_buff, int rc)
 */
int client_cleanup(int cli_socket, char *cmd_buff, char *rsp_buff, int rc){
    // Close socket if valid
    if (cli_socket > 0) {
        close(cli_socket);
    }

    // Free buffers
    if (cmd_buff) free(cmd_buff);
    if (rsp_buff) free(rsp_buff);

    return rc;
}