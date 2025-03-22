/**
 * @file server_options.c
 * @brief Implementation of server command-line options handling
 */

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_line_opts.h"

#define NUM_TABLES_DEFAULT (5)
#define OPENING_HOUR_DEFAULT (800)
#define CLOSING_HOUR_DEFAULT (2100)
#define PORT_DEFAULT (8000)
#define MENU_FILE_DEFAULT ("./menu.txt")
#define LOG_FILE_DEFAULT (stderr)
#define MIN_PORT_NUM (0)
#define MAX_PORT_NUM (65535)
#define FLAG_ON (1)
#define FLAG_OFF (0)
#define CLOSE_HR_MAX (2300)
#define CLOSE_HR_MIN (0100)
#define OPEN_HR_MAX (2300)
#define OPEN_HR_MIN (0)
#define MIDNIGHT_HOUR (0)
#define MIN_TABLES (1)
#define MINS_MODULO (100)

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
static bool validate_t_opt(const char *arg, int *num_of_tables)
{
    if (NULL == arg)
    {
        return true;
    }

    if (NULL == num_of_tables)
    {
        return false;
    }

    char *strtol_endptr;
    errno            = 0;
    long table_value = strtol(arg, &strtol_endptr, 10);

    if (ERANGE == errno)
    {
        printf("Error: Number out of range of long value\n");
        return false;
    }

    if ('\0' != *strtol_endptr)
    {
        printf("Error: Invalid characters in table value\n");
        return false;
    }

    if (MIN_TABLES > table_value || INT_MAX < table_value)
    {
        printf("Error: Table value must be between 1 and system's INT_MAX \n");
        return false;
    }

    *num_of_tables = (int)table_value;
    return true;
}

/**
 * Helper function to determine if closing time is in valid range and
 * handles the midnight (0000) special case since we're treating time
 * as integers.
 * @param opening_hour Opening hour value (0-2300)
 * @param closing_hour Closing hour value (0-2300, 0 means midnight)
 * @retval [true | false] for successful or failed validation.
 */
static bool is_valid_time_range(int opening_hour, int closing_hour)
{
    // Special case: Midnight (0) is a valid closing time
    if (closing_hour == MIDNIGHT_HOUR)
    {
        return true;
    }

    // Regular case: Closing hour must be after opening hour
    return closing_hour > opening_hour;
}

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
                           int        *c_flag)
{
    if (NULL == arg)
    {
        return true;
    }

    if (NULL == closing_hour || NULL == opening_hour || NULL == c_flag)
    {
        return false;
    }

    char *strtol_endptr;
    errno              = 0;
    long open_hr_value = strtol(arg, &strtol_endptr, 10);

    if (ERANGE == errno)
    {
        printf("Error: Number out of range of long value\n");
        return false;
    }

    if ('\0' != *strtol_endptr)
    {
        printf("Error: Invalid characters in opening hour value\n");
        return false;
    }

    if (OPEN_HR_MIN > open_hr_value || OPEN_HR_MAX < open_hr_value)
    {
        printf("Error: Opening hour must be between 0000 and 2300\n");
        return false;
    }

    if (0 != open_hr_value % MINS_MODULO)
    {
        printf("Time format is on the hour every hour minutes will always be "
               "'00'");
        return false;
    }

    if (FLAG_ON == *c_flag)
    {
        if (!is_valid_time_range(open_hr_value, *closing_hour))
        {
            printf("Error: Closing Time Cannot be before Opening Time\n");
            return false;
        }
    }

    *opening_hour = (int)open_hr_value;
    return true;
}

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
                           int        *o_flag)
{
    if (NULL == arg)
    {
        return true;
    }

    if (NULL == closing_hour || NULL == opening_hour || NULL == o_flag)
    {
        return false;
    }

    char *strtol_endptr;
    errno               = 0;
    long close_hr_value = strtol(arg, &strtol_endptr, 10);

    if (ERANGE == errno)
    {
        printf("Error: Number out of range of long value\n");
        return false;
    }

    if ('\0' != *strtol_endptr)
    {
        printf("Error: Invalid characters in opening hour value\n");
        return false;
    }

    if (close_hr_value < 0)
    {
        printf("Error: Closing hour cannot be negative\n");
        return false;
    }

    if (0 != (close_hr_value % MINS_MODULO))
    {
        printf("Time format is on the hour every hour minutes will always be "
               "'00'");
        return false;
    }

    if (CLOSE_HR_MAX < close_hr_value ||
        (CLOSE_HR_MIN > close_hr_value && MIDNIGHT_HOUR < close_hr_value))
    {
        printf("Error: Closing hour must be between 0100 and 2300, or 0000\n");
        return false;
    }

    if (FLAG_ON == *o_flag)
    {
        if (!is_valid_time_range(*opening_hour, close_hr_value))
        {
            printf("Error: Closing Time Cannot be before Opening Time\n");
            return false;
        }
    }

    *closing_hour = (int)close_hr_value;
    return true;
}

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
/**
 * @param arg
 * @param port_input
 * @retval [true | false] for successful or failed validation.
 * @brief
 * Validates, and sets the '-p' parameter from getopt
 * (or default value) to store the port number to be used by
 * the server.
 */
static bool validate_p_opt(const char *arg, int *port_input)
{
    if (NULL == arg)
    {
        return true;
    }

    if (NULL == port_input)
    {
        printf("Error: Invalid parameter for port value\n");
        return false;
    }

    char *strtol_endptr;
    errno               = 0;
    long port_num_value = strtol(arg, &strtol_endptr, 10);

    if (ERANGE == errno)
    {
        printf("Error: Number out of range of long value\n");
        return false;
    }

    if ('\0' != *strtol_endptr)
    {
        printf("Error: Invalid characters in port value\n");
        return false;
    }

    if (MIN_PORT_NUM > port_num_value || MAX_PORT_NUM < port_num_value)
    {
        printf("Error: Invalid Port Number Must be between 0 and 65535\n");
        return false;
    }

    *port_input = (int)port_num_value;
    return true;
}

/**
 * Helper function to validate file paths
 * Rejects empty strings and paths with wildcard characters
 * @param path The file path to validate
 * @retval [true | false] for successful or failed validation.
 */
static bool is_valid_file_path(const char *path)
{
    if ('\0' == path[0])
    {
        printf("Error: File path cannot be an empty string\n");
        return false;
    }

    if (0 == strcmp(path, "\"\"") || 0 == strcmp(path, "''"))
    {
        printf("Error: File path cannot be empty\n");
        return false;
    }

    const char *wildcard_chars = "*?=";
    for (size_t i = 0; i < strlen(wildcard_chars); i++)
    {
        if (strchr(path, wildcard_chars[i]) != NULL)
        {
            printf("Error: File path contains invalid wildcard or special "
                   "characters\n");
            return false;
        }
    }

    return true;
}

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
static bool validate_m_opt(const char *arg, char **menu_path)
{
    bool  result    = false;
    FILE *temp_file = NULL;

    if (NULL == arg)
    {
        result = true;
        goto cleanup;
    }

    if (NULL == menu_path)
    {
        printf("Error: Invalid parameter for menu path\n");
        goto cleanup;
    }

    if (!is_valid_file_path(arg))
    {
        goto cleanup;
    }

    temp_file = fopen(arg, "r");
    if (NULL == temp_file)
    {
        printf("Error: Menu file not found at %s\n", arg);
        goto cleanup;
    }

    *menu_path = strdup(arg);
    if (NULL == *menu_path)
    {
        printf("Error: Memory allocation failed for menu path\n");
        goto cleanup;
    }

    result = true;

cleanup:
    if (NULL != temp_file)
    {
        fclose(temp_file);
        temp_file = NULL;
    }

    return result;
}

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
static bool validate_l_opt(const char *arg, FILE **log_file)
{
    FILE *temp_file = NULL;

    if (NULL == arg)
    {
        return true;
    }

    if (NULL == log_file)
    {
        printf("Error: Invalid parameter for log file\n");
        return false;
    }

    if (!is_valid_file_path(arg))
    {
        return false;
    }

    temp_file = fopen(arg, "a+");
    if (NULL == temp_file)
    {
        printf("Error: Cannot open or create log file at %s\n", arg);
        return false;
    }

    *log_file = temp_file;
    return true;
}

/**
 * Displays help information for command line options menu
 */
static void display_help(void)
{
    printf("Usage: ./bin/pita_bytes [options]\n");
    printf("Options:\n");
    printf("  -t, --tables NUM    Number of tables (default: 5)\n");
    printf(
        "  -o, --open TIME     Opening hour in 24hr format (default: 0800)\n");
    printf(
        "  -c, --close TIME    Closing hour in 24hr format (default: 2100)\n");
    printf("  -p, --port PORT     Port number to listen on (default: 8000)\n");
    printf("  -m, --menu FILE     Path to menu file (default: ./menu.txt)\n");
    printf("  -l, --log FILE      Path to log file (default: stderr)\n");
    printf("  -h, --help          Display this help message\n");
}

int validate_and_set_options(int                 argc,
                             char               *argv[],
                             cmd_line_options_t *options)
{
    if (NULL == options)
    {
        fprintf(stderr, "Error: Invalid options parameter\n");
        return CMD_LINE_OPTS_FAILURE;
    }

    options->num_tables   = NUM_TABLES_DEFAULT;
    options->opening_hour = OPENING_HOUR_DEFAULT;
    options->closing_hour = CLOSING_HOUR_DEFAULT;
    options->port         = PORT_DEFAULT;
    options->menu_path    = (char *)MENU_FILE_DEFAULT;
    options->m_flag       = FLAG_OFF;
    options->log_file     = LOG_FILE_DEFAULT;

    int get_opt       = 0;
    int get_opt_index = 0;
    int c_flag        = 0;
    int o_flag        = 0;

    opterr = 0;

    static struct option long_options[] = {
        {"tables", optional_argument, 0, 't'},
        {"open", optional_argument, 0, 'o'},
        {"close", optional_argument, 0, 'c'},
        {"port", optional_argument, 0, 'p'},
        {"menu", optional_argument, 0, 'm'},
        {"log", optional_argument, 0, 'l'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    while ((get_opt = getopt_long(
                argc, argv, "t:o:c:p:m:l:h", long_options, &get_opt_index)) !=
           -1)
    {
        switch (get_opt)
        {
        case 't':
            if (!validate_t_opt(optarg, &options->num_tables))
            {
                return CMD_LINE_OPTS_FAILURE;
            }
            break;
        case 'o':
            if (!validate_o_opt(optarg,
                                &options->opening_hour,
                                &options->closing_hour,
                                &c_flag))
            {
                return CMD_LINE_OPTS_FAILURE;
            }
            o_flag = 1;
            break;
        case 'c':
            if (!validate_c_opt(optarg,
                                &options->closing_hour,
                                &options->opening_hour,
                                &o_flag))
            {
                return CMD_LINE_OPTS_FAILURE;
            }
            c_flag = 1;
            break;
        case 'p':
            if (!validate_p_opt(optarg, &options->port))
            {
                return CMD_LINE_OPTS_FAILURE;
            }
            break;
        case 'm':
            if (!validate_m_opt(optarg, &options->menu_path))
            {
                return CMD_LINE_OPTS_FAILURE;
            }
            options->m_flag = FLAG_ON;
            break;
        case 'l':
            if (!validate_l_opt(optarg, &options->log_file))
            {
                return CMD_LINE_OPTS_FAILURE;
            }
            break;
        case 'h':
            display_help();
            return CMD_LINE_OPTS_HELP;
        case ':':
            switch (optopt)
            {
            case 't':
                validate_t_opt(NULL, &options->num_tables);
                break;
            case 'o':
                validate_o_opt(NULL,
                               &options->opening_hour,
                               &options->closing_hour,
                               &c_flag);
                break;
            case 'c':
                validate_c_opt(NULL,
                               &options->closing_hour,
                               &options->opening_hour,
                               &o_flag);
                break;
            case 'p':
                validate_p_opt(NULL, &options->port);
                break;
            case 'm':
                validate_m_opt(NULL, &options->menu_path);
                break;
            case 'l':
                validate_l_opt(NULL, &options->log_file);
                break;
            }
            break;
        }
    }

    return CMD_LINE_OPTS_SUCCESS;
}

void cleanup_options(cmd_line_options_t *options)
{
    if (NULL == options)
    {
        return;
    }

    if (NULL != options->menu_path && FLAG_ON == options->m_flag)
    {
        free(options->menu_path);
        options->menu_path = NULL;
    }

    if (NULL != options->log_file && stderr != options->log_file)
    {
        fclose(options->log_file);
        options->log_file = NULL;
    }
}

/*** end of file ***/