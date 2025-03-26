
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "common.h"

/**
 * Initialize the module to have the cmd_line_opts configs
 * 
 * @param options Pointer to command line options structure
 * @return [true | false]
 */
bool protocol_init(server_state_t * server_state);

bool protocol_validate_header(const void * data, size_t message_size);