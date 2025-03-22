/**
 * @file syslog.h
 * @brief Header file for system logger.
 */

#ifndef SYSLOG_H
#define SYSLOG_H

typedef enum
{
    INFO,
    ERROR,
    CONN,
    USER,
    LOGIN,
    TYPE_COUNT
} log_type_t;

/**
 *
 */
bool syslog_init(FILE * log_file);

/**
 *
 */
bool syslog_write(FILE * log_file, log_type_t type, const char * custom_message);

/**
 *
 */
bool syslog_cleanup(void);

#endif /* SYSLOG_H */

/*** end of file ***/