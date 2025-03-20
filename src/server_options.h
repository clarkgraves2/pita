/**
 * @file server_options.h
 * @brief Header file for server command-line options handling
 */

 #ifndef SERVER_OPTIONS_H
 #define SERVER_OPTIONS_H
 
 #include <stdio.h>
 
 #define SERVER_OPTIONS_SUCCESS   (0)
 #define SERVER_OPTIONS_FAILURE   (-1)
 #define SERVER_OPTIONS_HELP      (-2)
 

/**
 * @struct server_options_t
 * @brief Configuration options for the server<br>
 * Hello<p>
 * @brief new
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
 * Free any resources allocated during options parsing
 * 
 * @param options Pointer to options structure
 */
void cleanup_options(server_options_t *options);

#endif /* SERVER_OPTIONS_H */