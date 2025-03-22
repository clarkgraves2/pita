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
    int server_socket_fd;
    int getaddrinfo_ret_val;
    int sockopt_val = 1;

    struct addrinfo *hints = calloc(1, sizeof(struct addrinfo));
    if (NULL == hints) 
    {
        fprintf(stderr, "hints memory allocation failed\n");
        return EXIT_FAILURE;
    }
    
    struct addrinfo *getaddr_res = NULL;
    
    char *get_addr_port_str = calloc(1, PORT_STR_BUFFER);
    if (NULL == get_addr_port_str) 
    {
        fprintf(stderr, "get_addr_port_str memory allocation failed\n");
        free(hints);
        return EXIT_FAILURE;
    }

    cmd_line_options_t *options = calloc(1, sizeof(cmd_line_options_t));
    if (NULL == options) 
    {
        fprintf(stderr, "cmd_line_options_t memory allocation failed\n");
        free(hints);
        free(get_addr_port_str);
        return EXIT_FAILURE;
    }

    int options_result = validate_and_set_options(argc, argv, options);

    if (options_result == CMD_LINE_OPTS_HELP)
    {
        cleanup_options(options);
        free(options);
        options = NULL;
        return EXIT_SUCCESS;
    }

    if (options_result == CMD_LINE_OPTS_FAILURE)
    {
       goto cleanup;
    }

    FILE * log_file = options->log_file;

    if(!syslog_init(log_file))
    {
        goto cleanup;
    }

    hints->ai_family = AF_INET;        
    hints->ai_socktype = SOCK_STREAM; 
    hints->ai_flags = AI_PASSIVE; 

    int written = snprintf(get_addr_port_str, PORT_STR_BUFFER, "%d", options->port);

    if (0 > written) 
    {
        syslog_write(log_file, ERROR, "int to str conversion failed\n");
        goto cleanup;
    }

    getaddrinfo_ret_val = getaddrinfo(NULL, get_addr_port_str, hints, &getaddr_res);
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

    

    syslog_write(log_file, INFO, "Server shutting down gracefully");
    freeaddrinfo(getaddr_res);
    close(server_socket_fd);
    cleanup_options(options);
    free(options);
    free(get_addr_port_str);
    free(hints);

    return EXIT_SUCCESS;

    cleanup:
    // Check each pointer before freeing
    if (hints != NULL) 
    {
        free(hints);
        hints = NULL;
    }
    if (get_addr_port_str != NULL) 
    {
        free(get_addr_port_str);
        get_addr_port_str = NULL;
    }
    if (options != NULL) 
    {
        cleanup_options(options);
        free(options);
        options = NULL;
    }
    if (getaddr_res != NULL) 
    {
        freeaddrinfo(getaddr_res);
        getaddr_res = NULL;
    }
    if (server_socket_fd >= 0) 
    {
        close(server_socket_fd);
    }
    
    return EXIT_FAILURE;
}

/*** end of file ***/