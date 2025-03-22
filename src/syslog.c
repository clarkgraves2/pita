
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "syslog.h"

#define SYSLOG_LOG_BUFFER          (1024)
#define SYSLOG_TIMESTAMP_SIZE      (32)
#define SYSLOG_TIMESTAMP_FORMAT ("%Y-%m-%d %H:%M:%S")

static const char *LOG_TYPE_STRINGS[TYPE_COUNT] = 
{
    "INFO",
    "ERROR",
    "CONN",
    "USER",
    "LOGIN",
};

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static char * assemble_log_message(log_type_t type, const char * custom_message)
{
    if(TYPE_COUNT < type)
    {
        fprintf(stderr, "Invalid Log Type");
        return NULL;
    }

    char * assembled_log[SYSLOG_LOG_BUFFER];

    time_t utc_time_now = time(NULL);
    if((time_t)-1 == utc_time_now)
    {
        fprintf(stderr, "Failed to get UTC Time");
        return NULL;
    }

    struct tm time_data;
    if (NULL == localtime_r(&utc_time_now, &time_data))
    {
        fprintf(stderr, "Failed to convert UTC time to Local Time");
        return NULL;
    }

    char timestamp[SYSLOG_TIMESTAMP_SIZE];
    if (0 == strftime(timestamp, sizeof(timestamp), SYSLOG_TIMESTAMP_FORMAT, &time_data))
    {
        fprintf(stderr, "Failed to format timestamp\n");
        return NULL;
    }

    int written_to_buffer = snprintf(assembled_log, SYSLOG_LOG_BUFFER, "[%s] [%s] %s\n",
                                    timestamp, LOG_TYPE_STRINGS[type], custom_message);

    if(0 > written_to_buffer || SYSLOG_LOG_BUFFER <= written_to_buffer)
    {
        fprintf(stderr, "Failed to assemble log message or log message size over limit");
        return NULL;
    }

    return assembled_log;
}

bool syslog_init(FILE *log_file)
{
    if (NULL == log_file)
    {
        fprintf(stderr, "Syslog_init log file failed to init");
        return false;
    }

    fprintf(stderr, "Syslog initiated successfully");

    return true;
}

bool syslog_write(FILE *log_file, log_type_t type, const char *custom_message)
{
    if (NULL == log_file || NULL == type || NULL == custom_message)
    {
        fprintf(stderr,
                "Syslog write parameters invalid, check write parameters");
        return false;
    }

    const char * formatted_log_message = assemble_log_message(type, custom_message);
    if (NULL == formatted_log_message)
    {
        fprintf(stderr, "assemble_log_message() failed");
        return false;
    }

    if (0 > fputs(formatted_log_message, log_file))
    {
        fprintf(stderr,"Failed to write formatted log message to log file");
        return false;
    }

    fflush(log_file);

    return true;
}

bool syslog_cleanup()
{
}