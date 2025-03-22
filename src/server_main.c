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
        // go to
    }

    getaddrinfo_ret_val = getaddrinfo(NULL, get_addr_port_str, &hints, &getaddr_res);
    if (0 != getaddrinfo_ret_val)
    {
        syslog_write(log_file, ERROR, "Failed to get address info");
        // go to
    }

    server_socket_fd = socket(getaddr_res->ai_family,getaddr_res->ai_socktype,getaddr_res->ai_protocol);
    if (SOCK_ASSIGN_ERR == server_socket_fd)
    {
        syslog_write(log_file, ERROR, "Failed to create socket");
        // go to
    }

    if (SETSOCKOPT_ERR == setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &sockopt_val, sizeof(sockopt_val)))
    {
        syslog_write(log_file, ERROR, "Failed to set socket options");
        // go to
    }

    if (BIND_ERR == bind(server_socket_fd, getaddr_res->ai_addr, getaddr_res->ai_addrlen))
    {
        syslog_write(log_file, ERROR, "Failed to bind socket");
        // go to
    }

    if (LISTEN_ERR == listen(server_socket_fd, SOMAXCONN))
    {
        syslog_write(log_file, ERROR, "Failed to listen on socket");
        // go to
    }

    fprintf(options.log_file, "Server shutting down gracefully\n");
    cleanup_options(&options);
    return EXIT_SUCCESS;

cleanup:
    syslog_cleanup();
    cleanup_options(&options);
    return EXIT_FAILURE;
}

/*** end of file ***/