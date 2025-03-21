/**
 * @file server_options.c
 * @brief Implementation of server command-line options handling
 */
 
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

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
#define CLOSE_HR_MAX (2300)
#define CLOSE_HR_MIN (0100)
#define OPEN_HR_MAX (2300)
#define OPEN_HR_MIN (0)
#define MIDNIGHT_HOUR (0)
#define MIN_TABLES (1)
#define MINS_MODULO (100)

static bool
validate_t_opt(const char * arg, int * num_of_tables)
{
    if(NULL == arg)
    {
        // log default num of tables set.
        return true;
    }

    if(NULL == num_of_tables)
    {
        // log error
        return false;
    }

    char * strtol_endptr;
    errno = 0;
    long table_value = strtol(arg, &strtol_endptr,10);

    if(ERANGE == errno)
    {
        // log error
        printf("Error: Number out of range of long value\n");
        return false;
    }

    if ('\0' != *strtol_endptr) 
    {
        // log error
        printf("Error: Invalid characters in table value\n");
        return false;
    }

    if (MIN_TABLES > table_value || INT_MAX < table_value) 
    {
        // log error
        printf("Error: Table value must be between 1 and system's INT_MAX \n");
        return false;
    }
    
    // log num of tables set
    *num_of_tables = (int)table_value;
    return true;
}

static bool
validate_o_opt(const char * arg, int * opening_hour, int * closing_hour, int * c_flag)
{
    if(NULL == arg)
    {
        return true;
    }
    
    if(NULL == closing_hour || NULL == opening_hour || NULL == c_flag)
    {
        // log error
        return false;
    }

    char * strtol_endptr;
    errno = 0;
    long open_hr_value = strtol(arg, &strtol_endptr,10);

    if(ERANGE == errno)
    {
        // log error
        printf("Error: Number out of range of long value\n");
        return false;
    }

    if ('\0' != *strtol_endptr) 
    {
        // log error
        printf("Error: Invalid characters in opening hour value\n");
        return false;
    }

    if (OPEN_HR_MIN > open_hr_value || OPEN_HR_MAX < open_hr_value) 
    {
        // log error
        printf("Error: Opening hour must be between 0000 and 2300\n");
        return false;
    }

    if(0 != open_hr_value % MINS_MODULO)
    {
        // log error
        printf("Time format is on the hour every hour minutes will always be '00'");
        return false;
    }
    
    if(FLAG_ON == *c_flag && *closing_hour > open_hr_value)
    {
        // log error
        printf("Error: Closing Time Cannot be before Opening Time\n");
        return false;
    }

    return true;
}

static bool
validate_c_opt(const char * arg, int * closing_hour, int * opening_hour, int * o_flag)
{
    if(NULL == arg)
    {
        return true;
    }

    
    if(NULL == closing_hour || NULL == opening_hour || NULL == o_flag)
    {
        // log error
        return false;
    }

    
    char * strtol_endptr;
    errno = 0;
    long close_hr_value = strtol(arg, &strtol_endptr,10);

    if(ERANGE == errno)
    {
        // log error
        printf("Error: Number out of range of long value\n");
        return false;
    }

    if ('\0' != *strtol_endptr)
    {
        // log error
        printf("Error: Invalid characters in opening hour value\n");
        return false;
    }

    if(0 != (close_hr_value % MINS_MODULO))
    {
        // log error
        printf("Time format is on the hour every hour minutes will always be '00'");
        return false;
    }
    
    if (CLOSE_HR_MAX < close_hr_value || (CLOSE_HR_MIN > close_hr_value && MIDNIGHT_HOUR < close_hr_value))
    {
        // log error
        printf("Error: Closing hour must be between 0100 and 2300, or 0000\n");
        return false;
    }

    if(FLAG_ON == *o_flag && *opening_hour > close_hr_value)
    {
        // log error
        printf("Error: Closing Time Cannot be before Opening Time\n");
        return false;
    }

    // log closing hour set
    *closing_hour = (int)close_hr_value;
    return true;
}

static bool
validate_p_opt(const char * arg, int * port_input)
{
    if(NULL == arg)
    {
        return true;
    }

    if(NULL == port_input)
    {
        // log error
        return false;
    }

    char * strtol_endptr;
    errno = 0;
    long port_num_value = strtol(arg, &strtol_endptr,10);

    if(ERANGE == errno)
    {
        // log error
        fprintf(stderr,"Error: Number out of range of strtol conversion.\n");
        return false;
    }

    if ('\0' != *strtol_endptr) 
    {
        // log error
        fprintf(stderr, "Error: Invalid characters in port value\n");
        return false;
    }

    if (MIN_PORT_NUM > port_num_value || MAX_PORT_NUM < port_num_value)
    {
        // log error
        fprintf(stderr, "Error: Invalid Port Number Must be between 0 and 65535\n");
        return false;
    }

    // log port set
    *port_input = (int)port_num_value;
    return true;
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
    if (NULL != temp_file)
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


int validate_and_set_options(int argc, char *argv[], cmd_line_options_t *options)
{
    if (NULL == options)
    {
        fprintf(stderr, "Error: Invalid options parameter\n");
        return CMD_LINE_OPTS_FAILURE;
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
                return CMD_LINE_OPTS_FAILURE;
            }
            break;
        case 'o':
            if (validate_o_opt(optarg, &options->opening_hour, &options->closing_hour, &c_flag) < 0) 
            {
                return CMD_LINE_OPTS_FAILURE;
            }
            o_flag = 1;
            break;
        case 'c':
            if (validate_c_opt(optarg, &options->closing_hour, &options->opening_hour, &o_flag) < 0) 
            {
                return CMD_LINE_OPTS_FAILURE;
            }
            c_flag = 1;
            break;
        case 'p':
            if (validate_p_opt(optarg, &options->port) < 0) 
            {
                return CMD_LINE_OPTS_FAILURE;
            }
            break;
        case 'm':
            if (NULL == validate_m_opt(optarg, &options->menu_path)) 
            {
                return CMD_LINE_OPTS_FAILURE;
            }
            break;
        case 'l':
            if (NULL == validate_l_opt(optarg, &options->log_file)) 
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
    
    return CMD_LINE_OPTS_SUCCESS;
}

void cleanup_options(cmd_line_options_t *options)
{
    if (NULL == options)
    {
        return;
    }
    
    if (options->menu_path != NULL && options->menu_path != MENU_FILE_DEFAULT)
    {
        free(options->menu_path);
        options->menu_path = NULL;
    }

    if (options->log_file != NULL && options->log_file != stderr)
    {
        fclose(options->log_file);
        options->log_file = NULL;
    }
}

/*** end of file ***/