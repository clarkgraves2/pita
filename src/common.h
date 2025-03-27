#ifndef COMMON_H
#define COMMON_H

#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct user_db user_db_t;
typedef struct reservation_system reservation_system_t;
struct cmd_line_options;

/**
 * Server state structure to centralize all server components
 */
typedef struct 
{
    FILE *log_file;

    int server_socket_fd;
    struct pollfd *poll_fds_array;
    int active_fds;
    char *incoming_data_buffer;

    struct cmd_line_options *options;
    user_db_t *user_database;
    reservation_system_t *reservation_system;
    
} server_state_t;

#endif /* COMMON_H */