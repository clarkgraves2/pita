/**
 * @file protocol.c
 * @brief Implementation of Pita-bytes protocol handlers
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "cmd_line_opts.h"
#include "protocol.h"
#include "syslog.h"

typedef struct __attribute__((packed))
{
    uint8_t op_code;
    union
    {
        uint8_t flag;
        uint8_t padding1;
    };
    uint16_t padding2;
    uint32_t session_id;
} header_t;

static cmd_line_options_t *protocol_configs = NULL;
static FILE * log_file = NULL;

static validate_user_command_header_fields()










/**
 * Initialize the module to have the cmd_line_opts configs
 * 
 * @param options Pointer to command line options structure
 * @return [true | false]
 */
bool protocol_init(cmd_line_options_t *options)
{
    if (NULL == options)
    {
        return false;
    }
    
    protocol_configs = options;
    log_file = protocol_configs->log_file;
    return true;
}

bool protocol_validate_header(const void * data, size_t message_size)
{
    if (sizeof(header_t) > message_size)
    {
        syslog(log_file, ERROR, "Message does not have a complete header.");
        return false;
    }

    const header_t * header = (const header_t *)data;



}