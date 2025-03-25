
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "cmd_line_opts.h"

/**
 * Initialize the module to have the cmd_line_opts configs
 * 
 * @param options Pointer to command line options structure
 * @return [true | false]
 */
bool protocol_init(cmd_line_options_t *options);

bool protocol_validate_header(const void * data, size_t message_size);