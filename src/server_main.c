/**
 * @file server_main.c
 * @brief Main file for the Pita-bytes Restaurant Reservation System
 */

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
#define RESPONSE_BUFFER_SIZE (4096)
#define MAX_LOG_MSG_LEN (100)

volatile sig_atomic_t serv_running = 1;
static char           response_buffer[RESPONSE_BUFFER_SIZE];

static void sigint_received(int sig)
{
    (void)sig;
    serv_running = 0;
}

static server_state_t *server_initialize(cmd_line_options_t *options)
{
    server_state_t *server_state = calloc(1, sizeof(server_state_t));
    if (server_state == NULL)
    {
        fprintf(stderr, "Failed to allocate server state\n");
        return NULL;
    }

    server_state->options  = options;
    server_state->log_file = options->log_file;

    if (!syslog_init(server_state->log_file))
    {
        fprintf(stderr, "Failed to initialize logging system\n");
        free(server_state);
        return NULL;
    }

    server_state->user_database = user_db_init(server_state);
    if (server_state->user_database == NULL)
    {
        syslog_write(server_state->log_file,
                     ERROR,
                     "Failed to initialize user database");
        syslog_cleanup();
        free(server_state);
        return NULL;
    }

    server_state->reservation_system = reserv_sys_init(server_state);
    if (server_state->reservation_system == NULL)
    {
        syslog_write(server_state->log_file,
                     ERROR,
                     "Failed to initialize reservation system");
        user_db_cleanup(server_state->user_database);
        syslog_cleanup();
        free(server_state);
        return NULL;
    }

    if (!protocol_init(server_state))
    {
        syslog_write(server_state->log_file,
                     ERROR,
                     "Failed to initialize protocol handler");
        reserv_sys_cleanup(server_state->reservation_system);
        user_db_cleanup(server_state->user_database);
        syslog_cleanup();
        free(server_state);
        return NULL;
    }

    syslog_write(
        server_state->log_file, INFO, "Server initialized successfully");
    return server_state;
}

static bool allocate_network_resources(server_state_t   *server_state,
                                       struct addrinfo **hints,
                                       char            **get_addr_port_str)
{
    if (server_state == NULL || hints == NULL || get_addr_port_str == NULL)
    {
        return false;
    }

    FILE *log_file = server_state->log_file;
    bool  result   = false;

    *hints = calloc(1, sizeof(struct addrinfo));
    if (*hints == NULL)
    {
        syslog_write(log_file, ERROR, "hints memory allocation failed");
        return false;
    }

    *get_addr_port_str = calloc(1, PORT_STR_BUFFER);
    if (*get_addr_port_str == NULL)
    {
        syslog_write(
            log_file, ERROR, "get_addr_port_str memory allocation failed");
        free(*hints);
        *hints = NULL;
        return false;
    }

    server_state->incoming_data_buffer = calloc(1, BUFFER_SIZE);
    if (server_state->incoming_data_buffer == NULL)
    {
        syslog_write(
            log_file, ERROR, "Failed to allocate incoming data buffer");
        free(*hints);
        *hints = NULL;
        free(*get_addr_port_str);
        *get_addr_port_str = NULL;
        return false;
    }

    server_state->poll_fds_array =
        calloc(NUM_OF_POLL_FDS, sizeof(struct pollfd));
    if (server_state->poll_fds_array == NULL)
    {
        syslog_write(
            log_file, ERROR, "Poll file descriptors failed to allocate");
        free(*hints);
        *hints = NULL;
        free(*get_addr_port_str);
        *get_addr_port_str = NULL;
        free(server_state->incoming_data_buffer);
        server_state->incoming_data_buffer = NULL;
        return false;
    }

    return true;
}

static bool setup_socket(server_state_t   *server_state,
                         int               port,
                         struct addrinfo  *hints,
                         char             *get_addr_port_str,
                         struct addrinfo **getaddr_res)
{
    FILE *log_file         = server_state->log_file;
    int   server_socket_fd = -1;
    int   sockopt_val      = 1;
    int   getaddrinfo_ret_val;

    hints->ai_family   = AF_INET;
    hints->ai_socktype = SOCK_STREAM;
    hints->ai_flags    = AI_PASSIVE;

    if (snprintf(get_addr_port_str, PORT_STR_BUFFER, "%d", port) < 0)
    {
        syslog_write(log_file, ERROR, "int to str conversion failed");
        return false;
    }

    getaddrinfo_ret_val =
        getaddrinfo(NULL, get_addr_port_str, hints, getaddr_res);
    if (getaddrinfo_ret_val != 0)
    {
        syslog_write(log_file, ERROR, "Failed to get address info");
        return false;
    }

    server_socket_fd = socket((*getaddr_res)->ai_family,
                              (*getaddr_res)->ai_socktype,
                              (*getaddr_res)->ai_protocol);
    if (server_socket_fd == SOCK_ASSIGN_ERR)
    {
        syslog_write(log_file, ERROR, "Failed to create socket");
        freeaddrinfo(*getaddr_res);
        *getaddr_res = NULL;
        return false;
    }

    if (setsockopt(server_socket_fd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &sockopt_val,
                   sizeof(sockopt_val)) == SETSOCKOPT_ERR)
    {
        syslog_write(log_file, ERROR, "Failed to set socket options");
        close(server_socket_fd);
        freeaddrinfo(*getaddr_res);
        *getaddr_res = NULL;
        return false;
    }

    if (bind(server_socket_fd,
             (*getaddr_res)->ai_addr,
             (*getaddr_res)->ai_addrlen) == BIND_ERR)
    {
        syslog_write(log_file, ERROR, "Failed to bind socket");
        close(server_socket_fd);
        freeaddrinfo(*getaddr_res);
        *getaddr_res = NULL;
        return false;
    }

    if (listen(server_socket_fd, SOMAXCONN) == LISTEN_ERR)
    {
        syslog_write(log_file, ERROR, "Failed to listen on socket");
        close(server_socket_fd);
        freeaddrinfo(*getaddr_res);
        *getaddr_res = NULL;
        return false;
    }

    server_state->server_socket_fd = server_socket_fd;
    return true;
}

static void init_poll_fds(server_state_t *server_state)
{
    if (server_state == NULL || server_state->poll_fds_array == NULL)
    {
        return;
    }

    server_state->poll_fds_array[0].fd     = server_state->server_socket_fd;
    server_state->poll_fds_array[0].events = POLLIN;
    server_state->active_fds               = 1;
}

static bool setup_network_state(server_state_t     *server_state,
                                cmd_line_options_t *options)
{
    if (server_state == NULL || options == NULL)
    {
        return false;
    }

    FILE            *log_file          = server_state->log_file;
    struct addrinfo *hints             = NULL;
    struct addrinfo *getaddr_res       = NULL;
    char            *get_addr_port_str = NULL;
    bool             result            = false;

    /* Allocate network resources */
    if (!allocate_network_resources(server_state, &hints, &get_addr_port_str))
    {
        syslog_write(log_file, ERROR, "Failed to allocate network resources");
        return false;
    }

    /* Set up socket */
    if (!setup_socket(server_state,
                      options->port,
                      hints,
                      get_addr_port_str,
                      &getaddr_res))
    {
        syslog_write(log_file, ERROR, "Failed to set up socket");
        goto cleanup;
    }

    /* Initialize poll file descriptors */
    init_poll_fds(server_state);

    /* Log server start */
    char log_msg[MAX_LOG_MSG_LEN];
    snprintf(
        log_msg, sizeof(log_msg), "Server listening on port %d", options->port);
    syslog_write(log_file, INFO, log_msg);

    result = true;

cleanup:
    if (hints != NULL)
    {
        free(hints);
    }

    if (get_addr_port_str != NULL)
    {
        free(get_addr_port_str);
    }

    if (getaddr_res != NULL)
    {
        freeaddrinfo(getaddr_res);
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
    if (server_state == NULL)
    {
        return false;
    }

    struct sockaddr_in client_addr = {0};
    socklen_t          client_len  = sizeof(client_addr);

    /* Check if maximum connections reached */
    if (server_state->active_fds >= NUM_OF_POLL_FDS)
    {
        syslog_write(
            server_state->log_file,
            ERROR,
            "Maximum connections reached, cannot accept new connection");

        /* Accept and immediately close connection */
        int temp_fd = accept(server_state->server_socket_fd,
                             (struct sockaddr *)&client_addr,
                             &client_len);
        if (temp_fd >= 0)
        {
            close(temp_fd);
        }
        return false;
    }

    /* Accept connection */
    int client_fd = accept(server_state->server_socket_fd,
                           (struct sockaddr *)&client_addr,
                           &client_len);
    if (client_fd < 0)
    {
        syslog_write(server_state->log_file,
                     ERROR,
                     "Failed to accept client connection");
        return false;
    }

    /* Log new connection */
    char log_msg[MAX_LOG_MSG_LEN];
    snprintf(log_msg,
             sizeof(log_msg),
             "New connection from %s:%d",
             inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));
    syslog_write(server_state->log_file, CONN, log_msg);

    /* Add to poll array */
    server_state->poll_fds_array[server_state->active_fds].fd     = client_fd;
    server_state->poll_fds_array[server_state->active_fds].events = POLLIN;
    server_state->active_fds++;

    return true;
}

static void remove_client(server_state_t *server_state, int index)
{
    if (server_state == NULL || index < 1 || index >= server_state->active_fds)
    {
        return;
    }

    close(server_state->poll_fds_array[index].fd);

    if (index < server_state->active_fds - 1)
    {
        server_state->poll_fds_array[index] =
            server_state->poll_fds_array[server_state->active_fds - 1];
    }

    server_state->active_fds--;
}

static bool process_client_data(server_state_t *server_state, int client_index)
{
    if (server_state == NULL || client_index < 1 ||
        client_index >= server_state->active_fds)
    {
        return false;
    }

    int client_fd = server_state->poll_fds_array[client_index].fd;

    /* Receive data from client */
    ssize_t bytes_received =
        recv(client_fd, server_state->incoming_data_buffer, BUFFER_SIZE, 0);

    if (bytes_received <= 0)
    {
        if (bytes_received == 0)
        {
            syslog_write(server_state->log_file, CONN, "Client disconnected");
        }
        else
        {
            syslog_write(server_state->log_file, ERROR, "recv() failed");
        }
        return false;
    }

    ssize_t response_len =
        protocol_process_message(server_state,
                                 client_fd,
                                 server_state->incoming_data_buffer,
                                 bytes_received,
                                 response_buffer,
                                 RESPONSE_BUFFER_SIZE);

    /* Handle processing error */
    if (response_len < 0)
    {
        syslog_write(
            server_state->log_file, ERROR, "Failed to process client message");
        return false;
    }

    ssize_t bytes_sent = send(client_fd, response_buffer, response_len, 0);

    if (bytes_sent != response_len)
    {
        syslog_write(server_state->log_file,
                     ERROR,
                     "Failed to send complete response to client");
        return false;
    }

    return true;
}

static void process_client_connections(server_state_t *server_state)
{
    if (server_state == NULL)
    {
        return;
    }

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
    if (server_state == NULL)
    {
        return;
    }

    while (serv_running)
    {

        int poll_count = poll(
            server_state->poll_fds_array, server_state->active_fds, WAIT_INDEF);

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
    if (server_state == NULL)
    {
        return;
    }

    syslog_write(
        server_state->log_file, INFO, "Server shutting down gracefully");

    for (int i = 1; i < server_state->active_fds; i++)
    {
        if (server_state->poll_fds_array != NULL &&
            server_state->poll_fds_array[i].fd >= 0)
        {
            close(server_state->poll_fds_array[i].fd);
        }
    }

    if (server_state->server_socket_fd >= 0)
    {
        close(server_state->server_socket_fd);
    }

    if (server_state->poll_fds_array != NULL)
    {
        free(server_state->poll_fds_array);
        server_state->poll_fds_array = NULL;
    }

    if (server_state->incoming_data_buffer != NULL)
    {
        free(server_state->incoming_data_buffer);
        server_state->incoming_data_buffer = NULL;
    }

    if (server_state->reservation_system != NULL)
    {
        reserv_sys_cleanup(server_state->reservation_system);
    }

    if (server_state->user_database != NULL)
    {
        user_db_cleanup(server_state->user_database);
    }

    cleanup_options(server_state->options);

    syslog_cleanup();
}

int main(int argc, char *argv[])
{
    struct sigaction sig_a       = {0};
    int              exit_status = EXIT_SUCCESS;

    cmd_line_options_t *options = calloc(1, sizeof(cmd_line_options_t));
    if (options == NULL)
    {
        fprintf(stderr, "Command line options memory allocation failed\n");
        return EXIT_FAILURE;
    }

    int options_result = validate_and_set_options(argc, argv, options);

    if (options_result == CMD_LINE_OPTS_FAILURE)
    {
        fprintf(stderr, "Command line validation failed\n");
        cleanup_options(options);
        free(options);
        return EXIT_FAILURE;
    }

    if (options_result == HELP_OPTION)
    {
        cleanup_options(options);
        free(options);
        return EXIT_SUCCESS;
    }

    server_state_t *server_state = server_initialize(options);
    if (server_state == NULL)
    {
        fprintf(stderr, "Server initialization failed\n");
        cleanup_options(options);
        free(options);
        return EXIT_FAILURE;
    }

    if (!setup_network_state(server_state, options))
    {
        syslog_write(
            server_state->log_file, ERROR, "Failed to set up network state");
        server_cleanup(server_state);
        free(server_state);
        return EXIT_FAILURE;
    }

    sig_a.sa_handler = sigint_received;
    if (sigaction(SIGINT, &sig_a, NULL) == SIGACTION_ERR)
    {
        syslog_write(
            server_state->log_file, ERROR, "Failed to register SIGINT handler");
        server_cleanup(server_state);
        free(server_state);
        return EXIT_FAILURE;
    }

    server_main_loop(server_state);

    server_cleanup(server_state);
    free(server_state);

    return exit_status;
}