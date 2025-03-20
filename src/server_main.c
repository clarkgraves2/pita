#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <regex.h>  

#define NUM_TABLES_DEFAULT (5)
#define OPENING_HOUR_DEFAULT (800)
#define CLOSING_HOUR_DEFAULT (2100)
#define PORT_DEFAULT (8000)
#define PARAM_ERR (-1)
#define TABLE_RANGE_ERR (-2)
#define STRTOL_CONV_ERR (-3)
#define TIME_RANGE_ERR (-4)
#define TIME_HAS_MINS_ERR (-5)
#define CLOSE_BF_OPEN_ERR (-6)

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
        return STRTOL_CONV_ERR;
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
        return STRTOL_CONV_ERR;
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
    if(c_flag != NULL && c_flag != 0 && closing_hour != NULL && *closing_hour < open_hr_value)
    {
        // log error
        printf("Error: Closing Time Cannot be before Opening Time\n");
        return CLOSE_BF_OPEN_ERR;
    }

    // log opening hour set
    *opening_hour = (int)open_hr_value;
    return (int)open_hr_value;
}

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

    if (close_hr_value > 2300 || (close_hr_value > 0000 && close_hr_value < 0100))
    {
        // log error
        printf("Error: Closing hour must between 0100 and 0000\n");
        return TIME_RANGE_ERR;
    }

    if(close_hr_value % 100 != 0)
    {
        // log error
        printf("Time format is on the hour every hour minutes will always be '00'");
        return TIME_HAS_MINS_ERR;
    }
    
    // Only do this check if closing hour has been set
    if(o_flag != NULL && o_flag != 0 && opening_hour != NULL && *opening_hour > close_hr_value)
    {
        // log error
        printf("Error: Closing Time Cannot be before Opening Time\n");
        return CLOSE_BF_OPEN_ERR;
    }

    // log opening hour set
    *closing_hour = (int)close_hr_value;
    return (int)close_hr_value;
}


int main(int argc, char *argv[])
{
    int get_opt = 0;
    int o_flag = 0;
    int c_flag = 0;
    int get_opt_index = 0;
    int num_of_tables = NUM_TABLES_DEFAULT;
    int opening_hour = OPENING_HOUR_DEFAULT;
    int closing_hour = CLOSING_HOUR_DEFAULT;
    int port_input = PORT_DEFAULT;
    opterr = 0;

    static struct option long_options[] = 
    {
        {"tables",  optional_argument, 0, 't'},
        {"open",    optional_argument, 0, 'o'},
        {"close",   optional_argument, 0, 'c'},
        {"help",    no_argument,       0, 'h'},
        {0,         0,                 0,  0 }
    };

    while ((get_opt = getopt_long(argc, argv, "t:o:c:h", long_options, &get_opt_index)) != -1) 
    {
        switch (get_opt) 
        {
        case 't':  
            if (validate_t_opt(optarg, &num_of_tables) < 0) 
            {
                return EXIT_FAILURE;
            }
            break;
        case 'o':
            if (validate_o_opt(optarg, &opening_hour, &closing_hour, &c_flag) < 0) 
            {
                return EXIT_FAILURE;
            }
            o_flag = 1;
            break;
        case 'c':
            if (validate_c_opt(optarg, &closing_hour, &opening_hour, &o_flag) < 0) 
            {
                return EXIT_FAILURE;
            }
            c_flag = 1;
            break;
        case 'p':
            printf("Option p was provided\n");
            break;
        case 'm':
            printf("Option m was provided\n");
            break;
        case 'l':
            printf("Option l was provided\n");
            break;
        case 'h':
            printf("Option h was provided\n");
            break;
        case ':':
            switch (optopt) 
            {
                case 't':
                    validate_t_opt(NULL, &num_of_tables);
                    break;
                case 'o':
                    validate_o_opt(NULL, &opening_hour, &closing_hour, &c_flag);
                    break;
                case 'c':
                    validate_c_opt(NULL, &closing_hour, &opening_hour, &o_flag);
                    break;
                case 'p':
                    validate_p_opt(NULL, &port_input);
                    break;
        
            }
        }
    }
    
    printf("Num of tables %d\n",num_of_tables);
    printf("Opening Hour %d\n",opening_hour);
   

    return 0;
}