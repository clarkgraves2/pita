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

#include "common.h"
#include "cmd_line_opts.h"
#include "user_db.h"
#include "reserv_sys.h"
#include "protocol.h"
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
#define HELP_OPTION (-2)

volatile sig_atomic_t serv_running;

static void sigint_received(int sig)
{
    (void)sig; 
    serv_running = 0;
}

server_state_t *
server_initialize(cmd_line_options_t * options)
{
    server_state_t *server_state = calloc(1, sizeof(server_state_t));
    if (NULL == server_state) 
    {
        fprintf(stderr, "Failed to allocate server state\n");
        return NULL;
    }

    server_state->log_file = options->log_file;


    if(!syslog_init(server_state->log_file))
    {
        goto cleanup;
    }

    if (!user_db_init(server_state)) 
    {
        goto cleanup;
    }
    
    if (!reserv_sys_init(server_state)) 
    {
        goto cleanup;
    }

    if (!protocol_init(server_state))
    {
        goto cleanup;
    }

    return server_state;

cleanup:
    if(server_state->reservation_system)
    {
        reserv_system_cleanup(server_state->reservation_system);
    }  

    if(server_state->user_database)
    {
        user_db_cleanup(server_state->user_database);
    }  

    if(server_state->log_file)
    {
        syslog_cleanup();
    }    

    if(options)
    {
        cleanup_options(options);
        options = NULL;
    }

    return NULL;
}   

static bool 
setup_network_state(server_state_t *server_state, cmd_line_options_t *options)
{
    FILE *log_file = server_state->log_file;
    int server_socket_fd = -1;
    int sockopt_val = 1;
    int getaddrinfo_ret_val;
    struct addrinfo *hints = NULL;
    struct addrinfo *getaddr_res = NULL;
    char *get_addr_port_str = NULL;
    bool result = false;

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

    server_socket_fd = socket(getaddr_res->ai_family, getaddr_res->ai_socktype, getaddr_res->ai_protocol);
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
    
    server_state->server_socket_fd = server_socket_fd;
    
    server_state->incoming_data_buffer = calloc(1, BUFFER_SIZE);
    if (NULL == server_state->incoming_data_buffer) 
    {
        syslog_write(log_file, ERROR, "Failed to allocate incoming data buffer");
        goto cleanup; 
    }

    /* Allocate poll file descriptors array */
    server_state->poll_fds_array = calloc(NUM_OF_POLL_FDS, sizeof(struct pollfd));
    if (NULL == server_state->poll_fds_array)
    {
        syslog_write(log_file, ERROR, "Poll file descriptors failed to allocate");
        goto cleanup;
    }

    /* Initialize the first poll fd with server socket */
    server_state->poll_fds_array[0].fd = server_state->server_socket_fd;
    server_state->poll_fds_array[0].events = POLLIN;
    server_state->active_fds = 1;
    
    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg), "Server listening on port %d", options->port);
    syslog_write(log_file, INFO, log_msg);
    
    result = true;
    
cleanup:
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
    
    if (NULL != getaddr_res) 
    {
        freeaddrinfo(getaddr_res);
        getaddr_res = NULL;
    }
    
    if (!result) 
    {
        if (SOCK_ASSIGN_ERR != server_socket_fd) 
        {
            close(server_socket_fd);
        }
        
        if (NULL != server_state->incoming_data_buffer)
        {
            free(server_state->incoming_data_buffer);
            server_state->incoming_data_buffer = NULL;
        }
        
        if (NULL != server_state->poll_fds_array)
        {
            free(server_state->poll_fds_array);
            server_state->poll_fds_array = NULL;
        }
    }
    
    return result;
}

int main(int argc, char *argv[])
{
    char *incoming_data_buffer = NULL;
    struct pollfd *poll_fds_array = NULL;
    struct sigaction sig_a = {0};

    cmd_line_options_t * options = calloc(1, sizeof(cmd_line_options_t));
    if (NULL == options) 
    {
        fprintf(stderr, "Command line options memory allocation failed\n");
        return EXIT_FAILURE;
    }

    int options_result = validate_and_set_options(argc, argv, options);

    if (CMD_LINE_OPTS_FAILURE == options_result)
    {
        fprintf(stderr, "Command line opt validation failed\n");
        goto cleanup;
    }
    
    if (HELP_OPTION == options_result)
    {
        cleanup_options(options);
        free(options);
        options = NULL;
        return EXIT_SUCCESS;
    }
    
    server_state_t * server_state = server_initialize(options);

    if (NULL == server_state)
    {
        fprintf(stderr, "Server subsystems failed to initialize\n");
        return EXIT_FAILURE;
    }

    FILE * log_file = server_state->log_file;

    if (!setup_network_state(server_state, options))
    {
        syslog_write(log_file, ERROR, "Failed to set up network state");
        goto cleanup;
    }

    serv_running = 1;
    sig_a.sa_handler = sigint_received;
    if (SIGACTION_ERR == (sigaction(SIGINT, &sig_a, NULL))) 
    {
        syslog_write(log_file, ERROR, "Failed to register SIGINT handler");
        goto cleanup;
    }

    while(serv_running)
    {
        int poll_count = poll(server_state->poll_fds_array, server_state->active_fds, WAIT_INDEF);

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
                int temp_fd = accept(server_state->server_socket_fd, (struct sockaddr*)&client_addr, &client_len);
                if (0 <= temp_fd) 
                {
                    close(temp_fd);
                }
                continue;
            }
            
            int client_fd = accept(server_state->server_socket_fd, (struct sockaddr*)&client_addr, &client_len);
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
    close(server_state->server_socket_fd);
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