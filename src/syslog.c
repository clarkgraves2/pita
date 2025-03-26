#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "syslog.h"

#define SYSLOG_LOG_BUFFER          (1024)
#define SYSLOG_TIMESTAMP_SIZE      (32)
#define SYSLOG_TIMESTAMP_FORMAT    ("%Y-%m-%d %H:%M:%S")

static const char *LOG_TYPE_STRINGS[TYPE_COUNT] = 
{
    "INFO",
    "ERROR",
    "CONN",
    "USER",
    "LOGIN",
};

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool assemble_log_message(log_type_t type, const char* custom_message, 
                                 char* buffer, size_t buffer_size)
{
    if ( TYPE_COUNT <= type || NULL == custom_message || NULL == buffer)
    {
        fprintf(stderr, "Syslog write parameters invalid, check write parameters\n");
        return false;
    }

    if (buffer_size < SYSLOG_LOG_BUFFER) 
    {
        fprintf(stderr, "Buffer size too small for log message\n");
        return false;
    }

    time_t utc_time_now = time(NULL);
    if ((time_t)-1 == utc_time_now)
    {
        fprintf(stderr, "Failed to get UTC Time\n");
        return false;
    }

    struct tm *time_result = localtime(&utc_time_now);
    if (time_result == NULL)
    {
        fprintf(stderr, "Failed to convert UTC time to Local Time\n");
        return false;
    }

   
    struct tm time_data = *time_result;

    char timestamp[SYSLOG_TIMESTAMP_SIZE];
    if (0 == strftime(timestamp, sizeof(timestamp), SYSLOG_TIMESTAMP_FORMAT, &time_data))
    {
        fprintf(stderr, "Failed to format timestamp\n");
        return false;
    }

    int written_to_buffer = snprintf(buffer, buffer_size, "[%s] [%s] %s\n",
                                    timestamp, LOG_TYPE_STRINGS[type], custom_message);


    if (0 > written_to_buffer || buffer_size < (size_t)written_to_buffer)
    {
        fprintf(stderr, "Failed to assemble log message or log message size over limit\n");
        return false;
    }

    return true;
}

bool syslog_init(FILE *log_file)
{
    if (NULL == log_file)
    {
        fprintf(stderr, "Syslog_init log file failed to init\n");
        return false;
    }

    if (0 > (fprintf(log_file, "%s", "")))
    {
        fprintf(stderr, "Log file is not writable\n");
        return false;
    }

    fprintf(log_file, "Syslog initialized successfully\n");
    if (0 != fflush(log_file)) 
    {
        fprintf(stderr, "Failed to flush log file\n");
        return false;
    }

    return true;
}

bool syslog_write(FILE *log_file, log_type_t type, const char *custom_message)
{
    if (NULL == log_file || type >= TYPE_COUNT || NULL == custom_message)
    {
        fprintf(stderr, "Syslog write parameters invalid, check write parameters\n");
        return false;
    }

    char *formatted_log = calloc(1, SYSLOG_LOG_BUFFER);
    if (NULL == formatted_log) 
    {
        fprintf(stderr, "Failed to allocate memory for log message\n");
        return false;
    }
    
    if (0 != pthread_mutex_lock(&log_mutex))
    {
        fprintf(stderr, "Failed to lock log mutex\n");
        free(formatted_log);
        formatted_log = NULL;
        return false;
    }
    
    bool format_success = assemble_log_message(type, custom_message, 
                                              formatted_log, SYSLOG_LOG_BUFFER);
    if (!format_success)
    {
        fprintf(stderr, "assemble_log_message() failed\n");
        goto cleanup;
    }
    
    if (0 > fputs(formatted_log, log_file))
    {
        fprintf(stderr, "Failed to write formatted log to log file\n");
        goto cleanup;
    }
    
    if (0 != fflush(log_file)) 
    {
        fprintf(stderr, "Failed to flush log file\n");
        goto cleanup;
    }

    free(formatted_log);
    formatted_log = NULL;
    pthread_mutex_unlock(&log_mutex);

    return true;

cleanup:
    free(formatted_log);
    formatted_log = NULL;
    pthread_mutex_unlock(&log_mutex);
    return false;
}

bool syslog_cleanup(void)
{
    int result = pthread_mutex_destroy(&log_mutex);
    if (0 != result)
    {
        fprintf(stderr, "Failed to destroy log mutex: %d\n", result);
        return false;
    }
    
    return true;
}

/*** end of file ***/