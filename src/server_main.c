// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "cmd_line_opts.h"
#include "syslog.h"

#define PORT_STR_BUFFER (6)
#define SOCK_ASSIGN_ERR (-1)
#define SETSOCKOPT_ERR (-1)
#define BIND_ERR (-1)
#define LISTEN_ERR (-1)
#define SIGACTION_ERR (-1)
#define WAIT_INDEF (-1)
#define NUM_OF_POLL_FDS (257)
#define BUFFER_SIZE (1024)

static volatile sig_atomic_t serv_running = 1;

static void sigint_received(int sig)
{
    // Standard supression of unused parameter warning from compiler
    // because sig is required by function signatures of signal handlers in C. 
    (void)sig; 
    serv_running = 0;
}

int main(int argc, char *argv[])
{
    int server_socket_fd = -1;
    int getaddrinfo_ret_val;
    int sockopt_val = 1;

    struct addrinfo *hints = NULL;
    struct addrinfo *getaddr_res = NULL;
    char *get_addr_port_str = NULL;
    cmd_line_options_t *options = NULL;
    char *incoming_data_buffer = NULL;
    struct pollfd *poll_fds_array = NULL;

    options = calloc(1, sizeof(cmd_line_options_t));
    if (NULL == options) 
    {
        fprintf(stderr, "cmd_line_options_t memory allocation failed\n");
        goto cleanup;
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

    hints = calloc(1, sizeof(struct addrinfo));
    if (NULL == hints) 
    {
        syslog_write(log_file, ERROR, "hints memory allocation failed\n");
        goto cleanup;
    }
    
    get_addr_port_str = calloc(1, PORT_STR_BUFFER);
    if (NULL == get_addr_port_str) 
    {
        syslog_write(log_file, ERROR, "get_addr_port_str memory allocation failed\n");
        goto cleanup;
    }

    hints->ai_family = AF_INET;        
    hints->ai_socktype = SOCK_STREAM; 
    hints->ai_flags = AI_PASSIVE; 

   
    if(0 > snprintf(get_addr_port_str, PORT_STR_BUFFER, "%d", options->port))
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

    struct sigaction sig_a = {0};
    sig_a.sa_handler = sigint_received;
    if (SIGACTION_ERR == (sigaction(SIGINT, &sig_a, NULL))) 
    {
        syslog_write(log_file, ERROR, "Failed to register SIGINT handler");
        goto cleanup;
    }
    
    incoming_data_buffer = calloc(1, BUFFER_SIZE);
    if (NULL == incoming_data_buffer) 
    {
        syslog_write(log_file, ERROR, "Failed to allocate incoming data buffer");
        goto cleanup; 
    }

    poll_fds_array = calloc(NUM_OF_POLL_FDS, sizeof(struct pollfd));
    if (NULL == poll_fds_array)
    {
        syslog_write(log_file,ERROR, "Poll file descriptors failed to allocate");
        goto cleanup;
    }

    int active_fds = 0;
    poll_fds_array[0].fd = server_socket_fd;
    poll_fds_array[0].events = POLLIN;
    active_fds = 1;

    while(serv_running)
    {
        int poll_count = poll(poll_fds_array, active_fds, WAIT_INDEF);

        // Why: To make sure that poll_count didn't error and more specifically
        // was the error from a signal received not the poll function. If so it'll continue
        // to the start of the loop to check if the signal's global variable has been changed.
        if (0 > poll_count) 
        {
            if (errno == EINTR) 
            {
                continue;
            }

            syslog_write(log_file, ERROR, "Poll count failed");
            break;
        }

        if(poll_fds_array[0].revents & POLLIN)
        {
            struct sockaddr_in client_addr = {0};
            socklen_t client_len = sizeof(client_addr);

            if (NUM_OF_POLL_FDS <= active_fds) 
            {
                syslog_write(log_file, ERROR, "Maximum connections reached, cannot accept new connection");
                
                // Why: We know that we've reached the limit of connection but the connection
                // is still in poll's queue. We have to accept it, fail it, and close it in order
                // for it to clear out of the queue so when poll runs again we don't get the same
                // connection that's in the queue. Also it's best practice for showing a rejected 
                // by doing this. We create a temp_fd to quickly do this "reject a connection."
                int temp_fd = accept(server_socket_fd, (struct sockaddr*)&client_addr, &client_len);
                if (0 <= temp_fd) 
                {
                    close(temp_fd);
                }
                continue;
            }
            
            int client_fd = accept(server_socket_fd, (struct sockaddr*)&client_addr, &client_len);
            if (0 > client_fd) 
            {
                syslog_write(log_file, ERROR, "Failed to accept() client connection");
                continue;
            }

            syslog_write(log_file, CONN, "New connection accepted");

            poll_fds_array[active_fds].fd = client_fd;
            poll_fds_array[active_fds].events = POLLIN;  
            active_fds++;
        }
        
        for (int idx = 1; idx < active_fds; idx++)
        {
            if (poll_fds_array[idx].revents & POLLIN)
            {
               ssize_t bytes_received = recv(poll_fds_array[idx].fd, incoming_data_buffer, BUFFER_SIZE, 0);

                if ( 0 >= bytes_received)
                {
                    if (0 == bytes_received)
                    {
                        syslog_write(log_file, CONN, "Client disconnected");
                    }
                    else
                    {
                        syslog_write(log_file, ERROR, "recv() failed");
                    }

                    close(poll_fds_array[idx].fd);
                    
                    // Why: When we close the file descriptor in the array that creates a gap in
                    // the file descriptor array. poll() function expects a contiguous array of 
                    // file descriptors so instead of shifting all elements we replace the gap
                    // with the last active element in the array. This is more efficient in O(1)
                    // vs. O(n) time complexity.
                    if (idx < active_fds - 1) 
                    {
                        poll_fds_array[idx] = poll_fds_array[active_fds - 1];
                        idx--; 
                    }

                    active_fds--;
                }
                else
                {
                  
                  // Implement partial read functionality
                  // Check for complete message and if
                  // error or not complete message 
                  // close connection and remove from array.
                }
            }
        }

    }


    syslog_write(log_file, INFO, "Server shutting down gracefully");
    free(incoming_data_buffer);
    incoming_data_buffer = NULL;
    free(poll_fds_array);
    poll_fds_array = NULL;
    close(server_socket_fd);
    freeaddrinfo(getaddr_res);
    getaddr_res = NULL;
    cleanup_options(options);
    free(options);
    options = NULL;
    free(get_addr_port_str);
    get_addr_port_str = NULL;
    free(hints);
    hints = NULL;

    return EXIT_SUCCESS;

cleanup:
    if(NULL != incoming_data_buffer)
    {
        free(incoming_data_buffer);
        incoming_data_buffer = NULL;
    }

    if (NULL != poll_fds_array)
    {
    free(poll_fds_array);
    poll_fds_array = NULL;
    }

    if (NULL != hints) 
    {
        free(hints);
        hints = NULL;
    }
    if (NULL != get_addr_port_str) 
    {
        free(get_addr_port_str);
        get_addr_port_str = NULL;
    }
    if (NULL != options) 
    {
        cleanup_options(options);
        free(options);
        options = NULL;
    }
    if (NULL != getaddr_res) 
    {
        freeaddrinfo(getaddr_res);
        getaddr_res = NULL;
    }
    if (0 <= server_socket_fd) 
    {
        close(server_socket_fd);
    }
    
    return EXIT_FAILURE;
}

/*** end of file ***/