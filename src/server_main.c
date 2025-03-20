#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "server_options.h"

int main(int argc, char *argv[])
{
    server_options_t options;
    
    int option_result = validate_and_set_options(argc, argv, &options);
    
    if (option_result == SERVER_OPTIONS_HELP) 
    {
        return EXIT_SUCCESS;
    }
    
    if (option_result == SERVER_OPTIONS_FAILURE) 
    {
        // Log Error
        return EXIT_FAILURE;
    }

  
    fprintf(options.log_file, "Server shutting down gracefully\n");
    cleanup_options(&options);
    
    return EXIT_SUCCESS;
}
