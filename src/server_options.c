/**
 * @file server_options.c
 * @brief Implementation of server command-line options handling
 */

 
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "server_options.h"

#define NUM_TABLES_DEFAULT (5)
#define OPENING_HOUR_DEFAULT (800)
#define CLOSING_HOUR_DEFAULT (2100)
#define PORT_DEFAULT (8000)
#define MENU_FILE_DEFAULT ("./menu.txt")
#define LOG_FILE_DEFAULT (stderr)

#define PARAM_ERR (-1)
#define TABLE_RANGE_ERR (-2)
#define STRTOL_CONV_ERR (-3)
#define TIME_RANGE_ERR (-4)
#define TIME_HAS_MINS_ERR (-5)
#define CLOSE_BF_OPEN_ERR (-6)
#define PORT_RANGE_ERR (-7)
#define INVALID_MENU_ERR (-8)
#define INVALID_CHAR_ERR (-9)

/**
 * Validate the number of tables option
 */
static int
validate_t_opt(const char * arg, int * num_of_tables)
{
    if(NULL == num_of_tables)
    {
        // log error
        return PARAM_ERR;
    }

    if(NULL == arg)
    {
        // log default num of tables set.
        *num_of_tables = NUM_TABLES_DEFAULT;
        return NUM_TABLES_DEFAULT;
    }

    char * strtol_endptr;
    errno = 0;
    long table_value = strtol(arg, &strtol_endptr,10);

    if(ERANGE == errno)
    {
        // log error
        printf("Error: Number out of range of long value\n");
        return STRTOL_CONV_ERR;
    }

    if (*strtol_endptr != '\0') {
        // log error
        printf("Error: Invalid characters in table value\n");
        return INVALID_CHAR_ERR;
    }

    if (table_value < 1 || table_value > INT_MAX) 
    {
        // log error
        printf("Error: Table value must be between 1 and system's INT_MAX \n");
        return TABLE_RANGE_ERR;
    }
    
    // log num of tables set
    *num_of_tables = (int)table_value;
    return (int)table_value;
}

/**
 * Validate the opening hour option
 */
static int
validate_o_opt(const char * arg, int * opening_hour, int * closing_hour, int * c_flag)
{
    if(NULL == opening_hour)
    {
        // log error
        return PARAM_ERR;
    }

    if(NULL == arg)
    {
        // log default opening hour set.
        *opening_hour = OPENING_HOUR_DEFAULT;
        return OPENING_HOUR_DEFAULT;
    }

    char * strtol_endptr;
    errno = 0;
    long open_hr_value = strtol(arg, &strtol_endptr,10);

    if(ERANGE == errno)
    {
        // log error
        printf("Error: Number out of range of long value\n");
        return STRTOL_CONV_ERR;
    }

    if (*strtol_endptr != '\0') {
        // log error
        printf("Error: Invalid characters in opening hour value\n");
        return INVALID_CHAR_ERR;
    }

    if (open_hr_value < 0 || open_hr_value > 2300) 
    {
        // log error
        printf("Error: Opening hour must be between 0000 and 2300\n");
        return TIME_RANGE_ERR;
    }

    if(open_hr_value % 100 != 0)
    {
        // log error
        printf("Time format is on the hour every hour minutes will always be '00'");
        return TIME_HAS_MINS_ERR;
    }
    
    // Only do this check if closing hour has been set
    if(c_flag != NULL && *c_flag != 0 && closing_hour != NULL && *closing_hour < open_hr_value)
    {
        // log error
        printf("Error: Closing Time Cannot be before Opening Time\n");
        return CLOSE_BF_OPEN_ERR;
    }

    // log opening hour set
    *opening_hour = (int)open_hr_value;
    return (int)open_hr_value;
}

/**
 * Validate the closing hour option
 */
static int
validate_c_opt(const char * arg, int * closing_hour, int * opening_hour, int * o_flag)
{
    if(NULL == closing_hour)
    {
        // log error
        return PARAM_ERR;
    }

    if(NULL == arg)
    {
        // log default opening hour set.
        *closing_hour = CLOSING_HOUR_DEFAULT;
        return CLOSING_HOUR_DEFAULT;
    }

    char * strtol_endptr;
    errno = 0;
    long close_hr_value = strtol(arg, &strtol_endptr,10);

    if(ERANGE == errno)
    {
        // log error
        printf("Error: Number out of range of long value\n");
        return STRTOL_CONV_ERR;
    }

    if (*strtol_endptr != '\0') {
        // log error
        printf("Error: Invalid characters in opening hour value\n");
        return STRTOL_CONV_ERR;
    }

    if (close_hr_value > 2300 || (close_hr_value < 0100 && close_hr_value > 0))
    {
        // log error
        printf("Error: Closing hour must be between 0100 and 2300, or 0000\n");
        return TIME_RANGE_ERR;
    }

    if(close_hr_value % 100 != 0)
    {
        // log error
        printf("Time format is on the hour every hour minutes will always be '00'");
        return TIME_HAS_MINS_ERR;
    }
    
    // Only do this check if opening hour has been set
    if(o_flag != NULL && *o_flag != 0 && opening_hour != NULL && *opening_hour > close_hr_value)
    {
        // log error
        printf("Error: Closing Time Cannot be before Opening Time\n");
        return CLOSE_BF_OPEN_ERR;
    }

    // log closing hour set
    *closing_hour = (int)close_hr_value;
    return (int)close_hr_value;
}

/**
 * Validate the port number option
 */
static int
validate_p_opt(const char * arg, int * port_input)
{
    if(NULL == port_input)
    {
        // log error
        return PARAM_ERR;
    }

    if(NULL == arg)
    {
        // log default port set.
        *port_input = PORT_DEFAULT;
        return PORT_DEFAULT;
    }

    char * strtol_endptr;
    errno = 0;
    long port_num_value = strtol(arg, &strtol_endptr,10);

    if(ERANGE == errno)
    {
        // log error
        printf("Error: Number out of range of strtol conversion.\n");
        return STRTOL_CONV_ERR;
    }

    if (*strtol_endptr != '\0') {
        // log error
        printf("Error: Invalid characters in port value\n");
        return INVALID_CHAR_ERR;
    }

    if (port_num_value < 0 || port_num_value > 65535)
    {
        // log error
        printf("Error: Invalid Port Number Must be between 0 and 65535\n");
        return PORT_RANGE_ERR;
    }

    // log port set
    *port_input = (int)port_num_value;
    return (int)port_num_value;
}

static bool
validate_m_opt(const char *arg, char **menu_path)
{
    bool result = false;
    FILE *temp_file = NULL;
    
    if (NULL == arg)
    {
        // log default menu file set
        result = true;
        goto cleanup;
    }

    if (NULL == menu_path)
    {
        // log error
        fprintf(stderr, "Error: Invalid parameter for menu path\n");
        goto cleanup;
    }

    temp_file = fopen(arg, "r");
    if (NULL == temp_file)
    {
        // log error
        fprintf(stderr, "Error: Menu file not found at %s\n", arg);
        goto cleanup;
    }

    *menu_path = strdup(arg);
    if (NULL == *menu_path)
    {
        // log error
        fprintf(stderr, "Error: Memory allocation failed for menu path\n");
        goto cleanup;
    }
    
    result = true;
    
cleanup:
    if (temp_file != NULL)
    {
        fclose(temp_file);
        temp_file = NULL;
    }
    
    return result;
}

static bool
validate_l_opt(const char *arg, FILE **log_file)
{
    bool result = false;
    FILE *temp_file = NULL;

    if (NULL == arg)
    {
        return true;
    }

    if (NULL == log_file)
    {
        fprintf(stderr, "Error: Invalid parameter for log file\n");
        return false;
    }

    

    temp_file = fopen(arg, "a+");
    if (NULL == temp_file)
    {
        fprintf(stderr, "Error: Cannot open or create log file at %s\n", arg);
        return false;
    }
    
    *log_file = temp_file;
    return true;
}

/**
 * Display help information
 */
static void
display_help(void)
{
    printf("Usage: ./bin/pita_bytes [options]\n");
    printf("Options:\n");
    printf("  -t, --tables NUM     Number of tables (default: 5)\n");
    printf("  -o, --open TIME      Opening hour in 24hr format (default: 0800)\n");
    printf("  -c, --close TIME     Closing hour in 24hr format (default: 2100)\n");
    printf("  -p, --port PORT      Port number to listen on (default: 8000)\n");
    printf("  -m, --menu FILE      Path to menu file (default: ./menu.txt)\n");
    printf("  -l, --log FILE       Path to log file (default: stderr)\n");
    printf("  -h, --help           Display this help message\n");
}


int validate_and_set_options(int argc, char *argv[], server_options_t *options)
{
    if (NULL == options)
    {
        fprintf(stderr, "Error: Invalid options parameter\n");
        return SERVER_OPTIONS_FAILURE;
    }
    
    options->num_tables = NUM_TABLES_DEFAULT;
    options->opening_hour = OPENING_HOUR_DEFAULT;
    options->closing_hour = CLOSING_HOUR_DEFAULT;
    options->port = PORT_DEFAULT;
    options->menu_path = MENU_FILE_DEFAULT;
    options->log_file = LOG_FILE_DEFAULT;
    
    int get_opt = 0;
    int o_flag = 0;
    int c_flag = 0;
    int get_opt_index = 0;
    opterr = 0;

    static struct option long_options[] = 
    {
        {"tables",  optional_argument, 0, 't'},
        {"open",    optional_argument, 0, 'o'},
        {"close",   optional_argument, 0, 'c'},
        {"port",    optional_argument, 0, 'p'},
        {"menu",    optional_argument, 0, 'm'},
        {"log",     optional_argument, 0, 'l'},
        {"help",    no_argument,       0, 'h'},
        {0,         0,                 0,  0 }
    };

    while ((get_opt = getopt_long(argc, argv, "t:o:c:p:m:l:h", long_options, &get_opt_index)) != -1) 
    {
        switch (get_opt) 
        {
        case 't':  
            if (validate_t_opt(optarg, &options->num_tables) < 0) 
            {
                return SERVER_OPTIONS_FAILURE;
            }
            break;
        case 'o':
            if (validate_o_opt(optarg, &options->opening_hour, &options->closing_hour, &c_flag) < 0) 
            {
                return SERVER_OPTIONS_FAILURE;
            }
            o_flag = 1;
            break;
        case 'c':
            if (validate_c_opt(optarg, &options->closing_hour, &options->opening_hour, &o_flag) < 0) 
            {
                return SERVER_OPTIONS_FAILURE;
            }
            c_flag = 1;
            break;
        case 'p':
            if (validate_p_opt(optarg, &options->port) < 0) 
            {
                return SERVER_OPTIONS_FAILURE;
            }
            break;
        case 'm':
            if (NULL == validate_m_opt(optarg, &options->menu_path)) 
            {
                return SERVER_OPTIONS_FAILURE;
            }
            break;
        case 'l':
            if (NULL == validate_l_opt(optarg, &options->log_file)) 
            {
                return SERVER_OPTIONS_FAILURE;
            }
            break;
        case 'h':
            display_help();
            return SERVER_OPTIONS_HELP;
        case ':':
            switch (optopt) 
            {
                case 't':
                    validate_t_opt(NULL, &options->num_tables);
                    break;
                case 'o':
                    validate_o_opt(NULL, &options->opening_hour, &options->closing_hour, &c_flag);
                    break;
                case 'c':
                    validate_c_opt(NULL, &options->closing_hour, &options->opening_hour, &o_flag);
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
    
    return SERVER_OPTIONS_SUCCESS;
}

void cleanup_options(server_options_t *options)
{
    if (NULL == options)
    {
        return;
    }
    
    if (options->log_file != NULL && options->log_file != stderr)
    {
        fclose(options->log_file);
        options->log_file = NULL;
    }
}