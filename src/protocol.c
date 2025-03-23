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

#define USER_AND_PASS_OFFSET (4)
#define TIME_AND_DATELEN_OFFSET (4)
#define USER_AND_PADD_OFFSET (4)
#define PADD_DATELEN_OFFSET (4)
#define USERNAME_MAX_LEN (256)
#define PASSWORD_MAX_LEN (256)
#define UINT16_FIELD_OFFSET (2)

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

static bool validate_user_command_fields(const header_t *header, const void *data, size_t length)
{
    if (0x02 < header->flag)
    {
        syslog_write(log_file, ERROR, "Invalid flag for user command message");
        return false;
    }

    if (sizeof(header_t) + USER_AND_PASS_OFFSET > length)
    {
        syslog_write(log_file, ERROR, "Header invalid, no username_len or password_len");
        return false;
    }

    const uint8_t *user_pass_data = (const uint8_t *)data + sizeof(header_t);
    uint16_t username_len = ntohs(*(uint16_t *)(user_pass_data));
    uint16_t password_len = ntohs(*(uint16_t *)(user_pass_data + UINT16_FIELD_OFFSET));
    
    if (sizeof(header_t) + USER_AND_PASS_OFFSET + username_len + password_len > length) 
    {
        syslog_write(log_file, ERROR, "Message recieved size not complete for user_pass length expected");
        return false;
    }
    
    if (USERNAME_MAX_LEN < username_len || PASSWORD_MAX_LEN < password_len) 
    {
        syslog_write(log_file, ERROR, "Username or password over the expected length");
        return false;
    }
    
    return true;
}

static bool validate_reservation_command_fields(const header_t *header, const void *data, size_t length)
{
    if (0x01 < header->flag)
    {
        syslog_write(log_file, ERROR, "Invalid flag for reservation command message");
        return false;
    }

    if((sizeof(header_t) + TIME_AND_DATELEN_OFFSET) > length)
    {
        syslog_write(log_file, ERROR, "Header invalid, no time or date_string_len sent");
        return false;
    }

    const uint8_t *time_date_data = (const uint8_t *)data + sizeof(header_t);
    uint16_t date_string_len = ntohs(*(uint16_t *)(time_date_data + UINT16_FIELD_OFFSET));

    if(sizeof(header_t) + TIME_AND_DATELEN_OFFSET + date_string_len > length)
    {
        syslog_write(log_file, ERROR, "Message recieved size not complete for time_date length expected");
        return false;
    }

    return true;
}

static bool validate_bookings_fields(const header_t *header, const void *data, size_t length)
{
    if((sizeof(header_t) + USER_AND_PADD_OFFSET) > length)
    {
        syslog_write(log_file, ERROR, "Header invalid, no optional username_len field or padding");
        return false;
    }

    return true;
}

static bool validate_list_fields(const header_t *header, const void *data, size_t length)
{
    if (sizeof(header_t) + PADD_DATELEN_OFFSET > length)
    {
        syslog_write(log_file, ERROR, "Header invalid, padding or date_string_len");
        return false;
    }

    const uint8_t *padd_date_data = (const uint8_t *)data + sizeof(header_t);
    uint16_t date_string_len = ntohs(*(uint16_t *)(padd_date_data + UINT16_FIELD_OFFSET));

    if(sizeof(header_t) + PADD_DATELEN_OFFSET + date_string_len > length)
    {
        syslog_write(log_file, ERROR, "Message recieved size not complete for date length expected");
        return false;
    }

    return true;
}

static bool validate_menu_fields(const header_t *header, const void *data, size_t length)
{
    if (sizeof(header_t) > length)
    {
        syslog_write(log_file, ERROR, "Message too short - incomplete header for menu");
        return false;
    }

    return true;
}








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