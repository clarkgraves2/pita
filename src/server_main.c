#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "cmd_line_opts.h"

int main(int argc, char *argv[])
{
    cmd_line_options_t options;

    int options_result = validate_and_set_options(argc, argv, &options);

    if (options_result == CMD_LINE_OPTS_HELP)
    {
        return EXIT_SUCCESS;
    }

    if (options_result == CMD_LINE_OPTS_FAILURE)
    {
        // Log Error
        return EXIT_FAILURE;
    }

    fprintf(options.log_file, "Server shutting down gracefully\n");
    cleanup_options(&options);

    return EXIT_SUCCESS;
}

/*** end of file ***/