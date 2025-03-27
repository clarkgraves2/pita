// NOLINTBEGIN(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#define _POSIX_C_SOURCE 200112L
#define _GNU_SOURCE
// NOLINTEND(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

#include <arpa/inet.h>
#include <errno.h>
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
#include "common.h"
#include "protocol.h"
#include "reserv_sys.h"
#include "syslog.h"
#include "user_db.h"

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
static FILE *log_file = NULL;

static bool allocate_network_resources(server_state_t *server_state,
                                     struct addrinfo **hints,
                                     char **get_addr_port_str);
                                     
static bool setup_socket(server_state_t *server_state,
                        int port,
                        struct addrinfo *hints,
                        char *get_addr_port_str,
                        struct addrinfo **getaddr_res);
                        
static void init_poll_fds(server_state_t *server_state);

static bool setup_network_state(server_state_t *server_state,
                              cmd_line_options_t *options);
                              
static void sigint_received(int sig);

static bool handle_new_connection(server_state_t *server_state);

static void remove_client(server_state_t *server_state, int index);

static bool process_client_data(server_state_t *server_state, int client_index);

static void process_client_connections(server_state_t *server_state);

static void server_main_loop(server_state_t *server_state);

static void server_cleanup(server_state_t *server_state);

static void sigint_received(int sig)
{
    (void)sig;
    serv_running = 0;
}

server_state_t *server_initialize(cmd_line_options_t *options)
{
    server_state_t *server_state = calloc(1, sizeof(server_state_t));
    if (NULL == server_state)
    {
        fprintf(stderr, "Failed to allocate server state\n");
        return NULL;
    }

    server_state->options = options;
    server_state->log_file = options->log_file;
    log_file = server_state->log_file;

    if (!syslog_init(server_state->log_file))
    {
        fprintf(stderr, "Failed to initialize logging system\n");
        goto cleanup;
    }

    server_state->user_database = user_db_init(server_state);
    if (NULL == server_state->user_database)
    {
        syslog_write(log_file, ERROR, "Failed to initialize user database");
        goto cleanup;
    }

    server_state->reservation_system = reserv_sys_init(server_state);
    if (NULL == server_state->reservation_system)
    {
        syslog_write(log_file, ERROR, "Failed to initialize reservation system");
        goto cleanup;
    }

    if (!protocol_init(server_state))
    {
        syslog_write(server_state->log_file,
                     ERROR,
                     "Failed to initialize protocol handler");
        goto cleanup;
    }

    syslog_write(
        server_state->log_file, INFO, "Server initialized successfully");
    return server_state;

cleanup:
    if (server_state != NULL)
    {
        if (server_state->reservation_system != NULL)
        {
            reserv_sys_cleanup(server_state->reservation_system);
        }

        if (server_state->user_database != NULL)
        {
            user_db_cleanup(server_state->user_database);
        }

        syslog_cleanup();
        free(server_state);
    }

    return NULL;
}

static bool allocate_network_resources(server_state_t *server_state,
                                     struct addrinfo **hints,
                                     char **get_addr_port_str)
{
    FILE *log_file = server_state->log_file;
    bool result = false;

    *hints = calloc(1, sizeof(struct addrinfo));
    if (NULL == *hints)
    {
        syslog_write(log_file, ERROR, "hints memory allocation failed");
        goto cleanup;
    }

    *get_addr_port_str = calloc(1, PORT_STR_BUFFER);
    if (NULL == *get_addr_port_str)
    {
        syslog_write(
            log_file, ERROR, "get_addr_port_str memory allocation failed");
        goto cleanup;
    }

    server_state->incoming_data_buffer = calloc(1, BUFFER_SIZE);
    if (NULL == server_state->incoming_data_buffer)
    {
        syslog_write(
            log_file, ERROR, "Failed to allocate incoming data buffer");
        goto cleanup;
    }

    server_state->poll_fds_array =
        calloc(NUM_OF_POLL_FDS, sizeof(struct pollfd));
    if (NULL == server_state->poll_fds_array)
    {
        syslog_write(
            log_file, ERROR, "Poll file descriptors failed to allocate");
        goto cleanup;
    }

    result = true;

cleanup:
    if (!result)
    {
        if (*hints != NULL)
        {
            free(*hints);
            *hints = NULL;
        }

        if (*get_addr_port_str != NULL)
        {
            free(*get_addr_port_str);
            *get_addr_port_str = NULL;
        }

        if (server_state->incoming_data_buffer != NULL)
        {
            free(server_state->incoming_data_buffer);
            server_state->incoming_data_buffer = NULL;
        }

        if (server_state->poll_fds_array != NULL)
        {
            free(server_state->poll_fds_array);
            server_state->poll_fds_array = NULL;
        }
    }

    return result;
}

static bool setup_socket(server_state_t *server_state,
                        int port,
                        struct addrinfo *hints,
                        char *get_addr_port_str,
                        struct addrinfo **getaddr_res)
{
    FILE *log_file = server_state->log_file;
    int server_socket_fd = -1;
    int sockopt_val = 1;
    int getaddrinfo_ret_val;
    bool result = false;

    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_flags = AI_PASSIVE;

    if (0 > snprintf(get_addr_port_str, PORT_STR_BUFFER, "%d", port))
    {
        syslog_write(log_file, ERROR, "int to str conversion failed");
        goto cleanup;
    }

    getaddrinfo_ret_val =
        getaddrinfo(NULL, get_addr_port_str, hints, getaddr_res);
    if (0 != getaddrinfo_ret_val)
    {
        syslog_write(log_file, ERROR, "Failed to get address info");
        goto cleanup;
    }

    server_socket_fd = socket((*getaddr_res)->ai_family,
                            (*getaddr_res)->ai_socktype,
                            (*getaddr_res)->ai_protocol);
    if (SOCK_ASSIGN_ERR == server_socket_fd)
    {
        syslog_write(log_file, ERROR, "Failed to create socket");
        goto cleanup;
    }

    if (SETSOCKOPT_ERR == setsockopt(server_socket_fd,
                                   SOL_SOCKET,
                                   SO_REUSEADDR,
                                   &sockopt_val,
                                   sizeof(sockopt_val)))
    {
        syslog_write(log_file, ERROR, "Failed to set socket options");
        goto cleanup;
    }

    if (BIND_ERR == bind(server_socket_fd,
                        (*getaddr_res)->ai_addr,
                        (*getaddr_res)->ai_addrlen))
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
    result = true;

cleanup:
    if (!result && server_socket_fd != SOCK_ASSIGN_ERR)
    {
        close(server_socket_fd);
    }

    return result;
}

static void init_poll_fds(server_state_t *server_state)
{
    server_state->poll_fds_array[0].fd = server_state->server_socket_fd;
    server_state->poll_fds_array[0].events = POLLIN;
    server_state->active_fds = 1;
}

static bool setup_network_state(server_state_t *server_state,
                              cmd_line_options_t *options)
{
    FILE *log_file = server_state->log_file;
    struct addrinfo *hints = NULL;
    struct addrinfo *getaddr_res = NULL;
    char *get_addr_port_str = NULL;
    bool result = false;

    if (!allocate_network_resources(server_state, &hints, &get_addr_port_str))
    {
        goto cleanup;
    }

    if (!setup_socket(server_state,
                    options->port,
                    hints,
                    get_addr_port_str,
                    &getaddr_res))
    {
        goto cleanup;
    }

    init_poll_fds(server_state);

    char log_msg[128];
    snprintf(
        log_msg, sizeof(log_msg), "Server listening on port %d", options->port);
    syslog_write(log_file, INFO, log_msg);

    result = true;

cleanup:
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

    if (getaddr_res != NULL)
    {
        freeaddrinfo(getaddr_res);
        getaddr_res = NULL;
    }

    if (!result)
    {
        if (server_state->incoming_data_buffer != NULL)
        {
            free(server_state->incoming_data_buffer);
            server_state->incoming_data_buffer = NULL;
        }

        if (server_state->poll_fds_array != NULL)
        {
            free(server_state->poll_fds_array);
            server_state->poll_fds_array = NULL;
        }
    }

    return result;
}

static bool handle_new_connection(server_state_t *server_state)
{
    struct sockaddr_in client_addr = {0};
    socklen_t client_len = sizeof(client_addr);

    if (NUM_OF_POLL_FDS <= server_state->active_fds)
    {
        syslog_write(server_state->log_file,
                   ERROR,
                   "Maximum connections reached, cannot accept new connection");

        int temp_fd = accept(server_state->server_socket_fd,
                           (struct sockaddr *)&client_addr,
                           &client_len);
        if (0 <= temp_fd)
        {
            close(temp_fd);
        }
        return false;
    }

    int client_fd = accept(server_state->server_socket_fd,
                         (struct sockaddr *)&client_addr,
                         &client_len);
    if (0 > client_fd)
    {
        syslog_write(server_state->log_file, ERROR, "Failed to accept() client connection");
        return false;
    }

    syslog_write(server_state->log_file, CONN, "New connection accepted");

    server_state->poll_fds_array[server_state->active_fds].fd = client_fd;
    server_state->poll_fds_array[server_state->active_fds].events = POLLIN;
    server_state->active_fds++;
    
    return true;
}

static void remove_client(server_state_t *server_state, int index)
{
    close(server_state->poll_fds_array[index].fd);

    if (index < server_state->active_fds - 1)
    {
        server_state->poll_fds_array[index] = server_state->poll_fds_array[server_state->active_fds - 1];
    }

    server_state->active_fds--;
}

static bool process_client_data(server_state_t *server_state, int client_index)
{
    int client_fd = server_state->poll_fds_array[client_index].fd;
    ssize_t bytes_received = recv(client_fd,
                               server_state->incoming_data_buffer,
                               BUFFER_SIZE,
                               0);

    if (0 >= bytes_received)
    {
        if (0 == bytes_received)
        {
            syslog_write(server_state->log_file, CONN, "Client disconnected");
        }
        else
        {
            syslog_write(server_state->log_file, ERROR, "recv() failed");
        }
        return false;
    }

   
    if (protocol_validate_header(server_state->incoming_data_buffer, bytes_received))
    {
        // TODO: Process the valid message
       
        syslog_write(server_state->log_file, INFO, "Valid message received");
        return true;
    }
    else
    {
        syslog_write(server_state->log_file, ERROR, "Invalid message received, closing connection");
        return false;
    }
}

static void process_client_connections(server_state_t *server_state)
{
    
    int idx = 1;
    
    while (idx < server_state->active_fds)
    {

        if (server_state->poll_fds_array[idx].revents & POLLIN)
        {
     
            if (!process_client_data(server_state, idx))
            {
                remove_client(server_state, idx);

                continue;
            }
        }
        
        idx++;
    }
}

static void server_main_loop(server_state_t *server_state)
{
    while (serv_running)
    {
        int poll_count = poll(server_state->poll_fds_array, 
                            server_state->active_fds, 
                            WAIT_INDEF);

   
        if (poll_count < 0)
        {
       
            if (errno == EINTR)
            {
                continue;
            }
            
            syslog_write(server_state->log_file, ERROR, "Poll failed");
            break;
        }
        
        
        if (server_state->poll_fds_array[0].revents & POLLIN)
        {
            handle_new_connection(server_state);
        }
        
    
        process_client_connections(server_state);
    }
}

static void server_cleanup(server_state_t *server_state)
{
    if (NULL == server_state)
    {
        return;
    }
    
    syslog_write(server_state->log_file, INFO, "Server shutting down gracefully");
    
    for (int i = 1; i < server_state->active_fds; i++)
    {
        if (server_state->poll_fds_array[i].fd >= 0)
        {
            close(server_state->poll_fds_array[i].fd);
        }
    }
    
    if (server_state->server_socket_fd >= 0)
    {
        close(server_state->server_socket_fd);
    }
    
    if (NULL != server_state->poll_fds_array)
    {
        free(server_state->poll_fds_array);
        server_state->poll_fds_array = NULL;
    }
    
    if (NULL != server_state->incoming_data_buffer)
    {
        free(server_state->incoming_data_buffer);
        server_state->incoming_data_buffer = NULL;
    }
    

    if (NULL != server_state->reservation_system)
    {
        reserv_sys_cleanup(server_state->reservation_system);
    }
    
    if (NULL != server_state->user_database)
    {
        user_db_cleanup(server_state->user_database);
    }
    
    cleanup_options(server_state->options);
    
    syslog_cleanup();
}

int main(int argc, char *argv[])
{
    struct sigaction sig_a = {0};

    cmd_line_options_t *options = calloc(1, sizeof(cmd_line_options_t));
    if (NULL == options)
    {
        fprintf(stderr, "Command line options memory allocation failed\n");
        return EXIT_FAILURE;
    }

    int options_result = validate_and_set_options(argc, argv, options);

    if (CMD_LINE_OPTS_FAILURE == options_result)
    {
        fprintf(stderr, "Command line opt validation failed\n");
        cleanup_options(options);
        free(options);
        return EXIT_FAILURE;
    }

    if (HELP_OPTION == options_result)
    {
        cleanup_options(options);
        free(options);
        return EXIT_SUCCESS;
    }

    server_state_t *server_state = server_initialize(options);
    if (NULL == server_state)
    {
        fprintf(stderr, "Server subsystems failed to initialize\n");
        cleanup_options(options);
        free(options);
        return EXIT_FAILURE;
    }

    if (!setup_network_state(server_state, options))
    {
        syslog_write(server_state->log_file, ERROR, "Failed to set up network state");
        server_cleanup(server_state);
        free(server_state);
        return EXIT_FAILURE;
    }

    serv_running = 1;
    sig_a.sa_handler = sigint_received;
    if (SIGACTION_ERR == (sigaction(SIGINT, &sig_a, NULL)))
    {
        syslog_write(server_state->log_file, ERROR, "Failed to register SIGINT handler");
        server_cleanup(server_state);
        free(server_state);
        return EXIT_FAILURE;
    }

    server_main_loop(server_state);

   
    server_cleanup(server_state);
    free(server_state);
    
    return EXIT_SUCCESS;
}