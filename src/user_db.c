#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_line_opts.h"
#include "user_db.h"
#include "syslog.h"

#define USERNAME_MAX_LEN (64)
#define PASSWORD_MAX_LEN (128)
#define MAX_USERS (256)
#define STRNCOMP_MATCH (0)

typedef struct 
{
    char username[USERNAME_MAX_LEN];
    char password[PASSWORD_MAX_LEN]; 
    bool is_admin;
    uint32_t session_id;  
    time_t session_creation_time; 
} user_t;

typedef struct
{
    user_t * users;
    size_t user_count;
    pthread_mutex_t db_lock;
}user_db_t;

static FILE * log_file = NULL;

bool user_db_register(user_db_t * user_database, const char *username, const char* password, bool is_admin)
{
    if (NULL == user_database || NULL == username || NULL == password)
    {
        syslog_write(log_file, ERROR, "user_db_register parameters invalid");
        return false;
    }

    if(0 != pthread_mutex_lock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "User_db_register mutex failed to lock");
        return false;
    }

    if(MAX_USERS <= user_database->user_count)
    {
        syslog_write(log_file, ERROR, "User capacity full");
        goto cleanup;
    }

    for(int idx = 0; idx < user_database->user_count; idx++)
    {
        if (STRNCOMP_MATCH == strncmp(user_database->users[idx].username, username, USERNAME_MAX_LEN))
        {
            syslog_write(log_file, ERROR, "Username already exists");
            goto cleanup;
        }
    }

    size_t username_len = strnlen(username, USERNAME_MAX_LEN);
    if (username_len == 0 || username_len >= USERNAME_MAX_LEN) 
    {
        syslog_write(log_file, ERROR, "Username empty or over 64 character limit");
        goto cleanup;
    }

    size_t password_len = strnlen(password, PASSWORD_MAX_LEN);
    if (password_len == 0 || password_len >= PASSWORD_MAX_LEN) 
    {
        syslog_write(log_file, ERROR, "Password empty or over 64 character limit");
        goto cleanup;
    }

    user_t * new_user = &user_database->users[user_database->user_count];
    
    // Justification for suppression: using a constant USERNAME_MAX_LEN that I checkagainst
    // and make sure it doesn't fail. Same reason for next clang-tidy supression below.
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    if(NULL == strncpy(new_user->username, username, USERNAME_MAX_LEN - 1))
    {
        syslog_write(log_file, ERROR, "Storing username strncopy failed");
        goto cleanup; 
    }

    new_user->username[USERNAME_MAX_LEN - 1] = '\0'; 
    
    // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling)
    if(NULL == strncpy(new_user->password, password, PASSWORD_MAX_LEN - 1))
    {
        syslog_write(log_file, ERROR, "Storing password strncopy failed");
        goto cleanup;
    }   

    new_user->password[PASSWORD_MAX_LEN - 1] = '\0';  

    new_user->is_admin = is_admin;
    new_user->session_id = 0;   
    
    user_database->user_count++;
    
    if(0 != pthread_mutex_unlock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "User_db_register mutex failed to unlock");
        return false;
    } 

    syslog_write(log_file, USER, "Sucessfully registered user");
    return true;

cleanup:
    if(0 != pthread_mutex_unlock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "User_db_register mutex failed to unlock");
        return false;
    } 
    return false;
}


bool user_db_init(user_db_t * user_database, cmd_line_options_t * userdb_configs)
{
    if (NULL == user_database || NULL == userdb_configs)
    {
        return false;
    }

    log_file = userdb_configs->log_file;

    user_database->user_count = 0;
    user_database->users = calloc(MAX_USERS, sizeof(user_t));
    if (NULL == user_database->users)
    {
        syslog_write(log_file, ERROR, "Failed to allocate memore for users in database");
        return false;
    }

    if (0 != pthread_mutex_init(&user_database->db_lock, NULL))
    {
        syslog_write(log_file, ERROR, "Database lock failed to initialize");
        free(user_database->users);
        user_database->users = NULL;
        return false;
    }
    
    if(!user_db_register(user_database, "admin", "password", true))
    {
    
        syslog_write(log_file, ERROR, "Failed to register admin");
        if(0!= pthread_mutex_destroy(&user_database->db_lock))
        {
            syslog_write(log_file, ERROR, "Failed to destroy lock after failing to register admin");
            free(user_database->users);
            user_database->users = NULL;
            return false;
        }

        free(user_database->users);
        user_database->users = NULL;
    
        return false;
    }

    return true;
}