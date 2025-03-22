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

int main(int argc, char *argv[])
{
    cmd_line_options_t options;
    int server_fd;
    int getaddrinfo_ret_val;
    struct addrinfo hints = {0};
    struct addrinfo *result = NULL;
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
        syslog_write(log_file, "int to str conversion failed\n");
        return EXIT_FAILURE;
    }

    getaddrinfo_ret_val = getaddrinfo(NULL, get_addr_port_str, &hints, &result);
    if (0 != getaddrinfo_ret_val)
    {
        syslog_write(log_file, ERROR, "Failed to get address info");
        // go to?
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