/**
 * @file server_options.h
 * @brief Header file for server command-line options handling
 */

#ifndef CMD_LINE_OPTS_H
#define CMD_LINE_OPTS_H

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#define CMD_LINE_OPTS_SUCCESS (0)
#define CMD_LINE_OPTS_FAILURE (-1)
#define CMD_LINE_OPTS_HELP (-2)

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
 * [int] m_flag
 *
 * [FILE*] log_file
 *
 */
typedef struct
{
    int   num_tables;
    int   opening_hour;
    int   closing_hour;
    int   port;
    char *menu_path;
    int   m_flag;
    FILE *log_file;
} cmd_line_options_t;

/**
 * Validates and sets server variables from command line arguments
 *
 * @param argc Argc from main
 * @param argv Argv arguments from main
 * @param options Pointer to cmd line options storage struct
 * @retval [CMD_LINE_OPTS_[SUCCESS | HELP | FAILURE]
 * @brief
 * Utilizes get_opt's loop parsing and switch statement to execute
 * desired validation functions. After the get_opt while loop finishes and
 * all arguments are validated, the resulting action is that the server can
 * proceed to initialize and have the validated cmd_line_args set.
 */
int validate_and_set_options(int                 argc,
                             char               *argv[],
                             cmd_line_options_t *options);

/**
 * Frees the resources used to validate / set server options
 *
 * @param options Pointer to options structure
 */
void cleanup_options(cmd_line_options_t *options);

#endif /* CMD_LINE_OPTS_H */

/*** end of file ***/