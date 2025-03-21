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
 * @param arg
 * @param num_of_tables
 * @retval [true | false] for sucessful or failed validation.
 * @brief
 * Validates, and sets the '-t' parameter from getopt
 * (or default value) to store the number of tables for the
 * reservation system.
 *
 * Process
 *
 * 1. Checks for early exit conditions that was to use the default value
 * from get_opt to save from continuing through unneeded validation process.
 *
 * 2. Check parameters haven't errored so that values provided by the
 * command line can be validated properly.
 *
 * 3. Uses strtol to get input into (long) integer type then performs two checks
 * provided by strtol to make sure we are in a valid int range and wont
 * overflow, and that there are not any invalid/special characters in the input
 * value.
 *
 * 4. Checks range of accepted tables between 1 and INT_MAX number to prevent
 * overflows and errors later in the reservation system.
 */
static bool validate_t_opt(const char *arg, int *num_of_tables);

/**
 * @param arg
 * @param opening_hour
 * @param closing_hour
 * @param c_flag
 * @retval [true | false] for sucessful or failed validation.
 * @brief
 * Validates, and sets the '-o' parameter from getopt
 * (or default value) to store the opening hour of the reservation
 * system.
 *
 * Process
 *
 * 1. Checks for early exit conditions that was to use the default value
 * from get_opt to save from continuing through unneeded validation process.
 *
 * 2. Need to make sure parameters haven't errored so that values provided by
 * the command line can be validated properly.
 *
 * 3. Uses strtol to get input into (long) integer type then performs two checks
 * provided by strtol to make sure we are in a valid int range and wont
 * overflow, and that there are not any invalid/special characters in the input
 * value.
 *
 * 4. Check the opening hour provided is in the valid range of closing hours.
 *
 * 5. Check that closing hour isn't before the opening hour so that later when
 * listing and manipulating reservations we won't get unexpected behavior.
 */
static bool validate_o_opt(const char *arg,
                           int        *opening_hour,
                           int        *closing_hour,
                           int        *c_flag);

/**
 * @param arg
 * @param closing_hour
 * @param opening_hour
 * @param o_flag
 * @retval [true | false] for sucessful or failed validation.
 * @brief
 * Validates, and sets the '-c' parameter from getopt
 * (or default value) to store the closing hour of the reservation
 * system.
 *
 * Process
 *
 * 1. Checks for early exit conditions that was to use the default value
 * from get_opt to save from continuing through unneeded validation process.
 *
 * 2. Need to make sure parameters haven't errored so that values provided by
 * the command line can be validated properly.
 *
 * 3. Uses strtol to get input into (long) integer type then performs two checks
 * provided by strtol to make sure we are in a valid int range and wont
 * overflow, and that there are not any invalid/special characters in the input
 * value.
 *
 * 4. Makes sure the closing hour provided is in the valid range of closing
 * hours.
 *
 * 5. Want to make sure closing hour isn't before the opening hour so that later
 * when listing and manipulating reservations we won't get unexpected behavior.
 */
static bool validate_c_opt(const char *arg,
                           int        *closing_hour,
                           int        *opening_hour,
                           int        *o_flag);

/**
 * @param arg
 * @param port_input
 * @retval [true | false] for sucessful or failed validation.
 * @brief
 * Validates, and sets the '-p' parameter from getopt
 * (or default value) to store the port number to be used by
 * the server.
 *
 * Process
 *
 * 1. Checks for early exit condition that the default value already set in
 * main by get_opt is to be used.
 *
 * 2. Checks to make sure that the variable we're using in main (default value)
 * is valid for us to store once the option is validated.
 *
 * 3. Uses strol to convert the command line option (string) into a long
 * integer, it then checks the range of the converted string by checking if
 * strtol set errno to ERANGE.
 *
 * 4. Check the strtol_endptr set by strtol and if it's the NULL terminated that
 * means that the value (string) provided contained only numbers and no
 * characters or special characters.
 *
 * 5. Need to make sure the validated number is a valid port number in the
 * accepted range because if it's not a valid port it would cause
 * errors/potential crashes in the server.
 */
static bool validate_p_opt(const char *arg, int *port_input);

/**
 * @param arg
 * @param menu_path
 * @retval [true | false] for sucessful or failed validation
 * @brief
 * Validates, and sets the '-m' parameter from getopt
 * (or default value) to store the file_path of the menu
 * in the server's option settings.
 *
 * Process
 *
 * 1. If no value was provided after the option get_opt will set value
 * to NULL so that the default file in main will be used resulting in
 * an early exit.
 *
 * 2. Checks that menu file_path didn't fail when passing by reference.
 *
 * 3. If fopen fails on the menu's filepath then it's an invalid path.
 *
 * 4. If the filepath is valid we store the file_path in the server settings
 * close the file since it's a read only menu and isn't used in our program.
 * We then set it's value to NULL after it's closed.
 *
 */
static bool validate_m_opt(const char *arg, char **menu_path);

/**
 * @param arg
 * @param log_file
 * @retval [true | false] for sucessful or failed validation
 * @brief
 * Validates, and sets the '-l' parameter from getopt
 * (or default value to open the log file and store the
 * pointer in the server's option settings
 *
 * Process
 *
 * 1.If no value was provided after the option get_opt will set value
 * to NULL so that the default file in main will be used resulting in
 * an early exit.
 *
 * 2. Checks that log_file pointer didn't fail when passing by reference.
 *
 * 3. If fopen fails to open the provided file_path then that tells us that
 * it was an invalid file_path or non-existent file.
 *
 * 4. If the file_path is validated we set the opened log_file pointer
 * to be used by the server for logging.
 *
 */
static bool validate_l_opt(const char *arg, FILE **log_file);

/**
 * Displays help information for command line options menu
 */
static void display_help(void);

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