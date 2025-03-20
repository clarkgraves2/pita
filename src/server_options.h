/**
 * @file server_options.h
 * @brief Header file for server command-line options handling
 */

#ifndef SERVER_OPTIONS_H
#define SERVER_OPTIONS_H

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>

#define SERVER_OPTIONS_SUCCESS   (0)
#define SERVER_OPTIONS_FAILURE   (-1)
#define SERVER_OPTIONS_HELP      (-2)
 
/**
 * [int] num_tables 
 * 
 * [int] opening_hour 
 * 
 * [int] closing_hour
 * 
 * [int] port
 * 
 * [char*] menu_path
 * 
 * [FILE*] log_file
 * 
 */
typedef struct 
{
    int num_tables;   
    int opening_hour;    
    int closing_hour;    
    int port;            
    char *menu_path;
    FILE *log_file;      
} server_options_t;

/**
 * Validate the number of tables option
 */
static int
validate_t_opt(const char * arg, int * num_of_tables);

/**
 * Validate the opening hour option
 */
static int
validate_o_opt(const char * arg, int * opening_hour, int * closing_hour, int * c_flag);

/**
 * Validate the closing hour option
 */
static int
validate_c_opt(const char * arg, int * closing_hour, int * opening_hour, int * o_flag);

/**
 * Validate the port number option
 */
static int
validate_p_opt(const char * arg, int * port_input);

/**
 * Validate the menu file path option
 */
static char *
validate_m_opt(const char *arg, char **menu_path);

/**
 * @brief Parses, validates, and sets the -l parameter from getopt
 * to use as the log file path for the server's settings
 * @param arg
 * @param log_file
 */
static bool validate_l_opt(const char *arg, FILE **log_file);

/**
 * Displays help information
 */
static void display_help(void);

/**
 * Validates and sets server variables from command line arguments
 * 
 * @param argc Argc from main
 * @param argv Argv arguments from main
 * @param options Pointer to options structure used by the server
 * @retval SERVER_OPTIONS_SUCCESS 
 * @retval SERVER_OPTIONS_HELP 
 * @retval SERVER_OPTIONS_FAILURE
 * 
 */
int validate_and_set_options(int argc, char *argv[], server_options_t *options);

/**
 * Frees the resources used to validate / set server options
 * 
 * @param options Pointer to options structure
 */
void cleanup_options(server_options_t *options);

#endif /* SERVER_OPTIONS_H */