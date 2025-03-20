#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <regex.h>  

#define NUM_TABLES_DEFAULT (5)
#define PARAM_ERR (-1)
#define TABLE_RANGE_ERR (-2)
#define STRTOL_CONV_ERR (-3)

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

int main(int argc, char *argv[])
{
    int get_opt = 0;
    int get_opt_index = 0;
    int num_of_tables = NUM_TABLES_DEFAULT;
    opterr = 0;

    static struct option long_options[] = 
    {
        {"tables",  optional_argument, 0, 't'},
        {"help",    no_argument,       0, 'h'},
        {0,         0,                 0,  0 }
    };

    while ((get_opt = getopt_long(argc, argv, "t:h", long_options, &get_opt_index)) != -1) 
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
            printf("Option o was provided\n");
            break;
        case 'c':
            printf("Option c was provided\n");
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
        
            }
        }
    }
    
    printf("Num of tables %d\n",num_of_tables);

    return 0;
}