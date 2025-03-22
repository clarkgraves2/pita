#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "cmd_line_opts.h"
#include "syslog.h"

#define PORT_STR_BUFFER (6)
#define SOCK_ASSIGN_ERR (-1)
#define SETSOCKOPT_ERR (-1)
#define BIND_ERR (-1)
#define LISTEN_ERR (-1)
#define BUFFER_SIZE (1024)

static volatile sig_atomic_t serv_running = 1;

int main(int argc, char *argv[])
{
    cmd_line_options_t options;
    int server_socket_fd;
    int getaddrinfo_ret_val;
    int sockopt_val = 1;
    struct addrinfo hints = {0};
    struct addrinfo *getaddr_res = NULL;
    char get_addr_port_str[PORT_STR_BUFFER];

    int options_result = validate_and_set_options(argc, argv, &options);

    if (options_result == CMD_LINE_OPTS_HELP)
    {
        cleanup_options(&options);
        return EXIT_SUCCESS;
    }

    if (options_result == CMD_LINE_OPTS_FAILURE)
    {
       goto cleanup;
    }

    FILE * log_file = options.log_file;

    if(!syslog_init(log_file))
    {
        goto cleanup;
    }

    hints.ai_family = AF_INET;        
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE; 

    int written = snprintf(get_addr_port_str, sizeof(get_addr_port_str), "%d", options.port);

    if (0 > written) 
    {
        syslog_write(log_file, ERROR, "int to str conversion failed\n");
        goto cleanup;
    }

    getaddrinfo_ret_val = getaddrinfo(NULL, get_addr_port_str, &hints, &getaddr_res);
    if (0 != getaddrinfo_ret_val)
    {
        syslog_write(log_file, ERROR, "Failed to get address info");
        goto cleanup;
    }

    server_socket_fd = socket(getaddr_res->ai_family,getaddr_res->ai_socktype,getaddr_res->ai_protocol);
    if (SOCK_ASSIGN_ERR == server_socket_fd)
    {
        syslog_write(log_file, ERROR, "Failed to create socket");
        goto cleanup;
    }

    if (SETSOCKOPT_ERR == setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt_val, sizeof(sockopt_val)))
    {
        syslog_write(log_file, ERROR, "Failed to set socket options");
        goto cleanup;
    }

    if (BIND_ERR == bind(server_socket_fd, getaddr_res->ai_addr, getaddr_res->ai_addrlen))
    {
        syslog_write(log_file, ERROR, "Failed to bind socket");
        goto cleanup;
    }

    if (LISTEN_ERR == listen(server_socket_fd, SOMAXCONN))
    {
        syslog_write(log_file, ERROR, "Failed to listen on socket");
        goto cleanup;
    }

    while (serv_running)
    {
        // Accept connection
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_socket_fd, (struct sockaddr*)&client_addr, &addr_len);
        
        if (client_fd < 0)
        {
            if (EINTR == errno)
            {
    
                continue;
            }
            syslog_write(log_file, ERROR, "Accept failed");
            continue;
        }
        
   
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        
        char log_msg[BUFFER_SIZE];
        snprintf(log_msg, sizeof(log_msg), "Connection from %s:%d", 
                 client_ip, ntohs(client_addr.sin_port));
        syslog_write(log_file, CONN, log_msg);
        printf("New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        
   
        char buffer[BUFFER_SIZE] = {0};
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_read > 0)
        {
            buffer[bytes_read] = '\0';
            printf("Received: %s\n", buffer);
            
            const char *response = "Hello from server!\n";
            send(client_fd, response, strlen(response), 0);
        }
        

        close(client_fd);
    }
    
    close(server_socket_fd);
    
    syslog_write(log_file, INFO, "Server shutting down gracefully");
    cleanup_options(&options);
    return EXIT_SUCCESS;

cleanup:
    if (NULL != getaddr_res)
    {
        freeaddrinfo(getaddr_res);
    }

    if (0 <= server_socket_fd)
    {
        close(server_socket_fd);
    }
    cleanup_options(&options);
    return EXIT_FAILURE;
}

/*** end of file ***/