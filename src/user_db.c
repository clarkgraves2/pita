#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmd_line_opts.h"
#include "user_db.h"
#include "syslog.h"

#define USERNAME_MAX_LEN (64)
#define PASSWORD_MAX_LEN (128)
#define MAX_USERS (256)
#define STRNCMP_MATCH (0)
#define SESSION_CHECK_SEC_INTERVAL (60)
#define SESSION_TIMEOUT (300)

typedef struct 
{
    char username[USERNAME_MAX_LEN];
    char password[PASSWORD_MAX_LEN]; 
    bool is_admin;
    uint32_t session_id;  
    time_t session_creation_time; 
    time_t last_activity_time;
} user_t;

typedef struct
{
    user_t * users;
    size_t user_count;
    pthread_mutex_t db_lock;
}user_db_t;

static FILE * log_file = NULL;

static uint32_t generate_new_session_id(user_db_t *user_database)
{
    uint32_t new_session_id = (uint32_t)time(NULL);
    
    // Why: To have the best chance of uniqueness we take the current
    // time and XOR it with a random number. This exponientially decreases
    // the chances of duplicate session id's, as well as squashes predictability
    // is someone was trying to hijack our session id.
    new_session_id ^= (uint32_t)rand();
    
    while (new_session_id == 0)
    {
        new_session_id = (uint32_t)time(NULL) ^ (uint32_t)rand();
    }
    
    bool unique = false;
    while (!unique)
    {
        unique = true;
        for (size_t idx = 0; idx < user_database->user_count; idx++)
        {
            if (user_database->users[idx].session_id == new_session_id)
            {
                unique = false;
                new_session_id = (uint32_t)time(NULL) ^ (uint32_t)rand();
                break;
            }
        }
    }
    
    return new_session_id;
}

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

    for(size_t idx = 0; idx < user_database->user_count; idx++)
    {
        if (STRNCMP_MATCH == strncmp(user_database->users[idx].username, username, USERNAME_MAX_LEN))
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

bool user_db_delete_user(user_db_t *user_database, const char *username, uint32_t admin_session_id)
{
    if (NULL == user_database || NULL == username)
    {
        syslog_write(log_file, ERROR, "user_db_delete_user parameters invalid");
        return false;
    }

    if (0 != pthread_mutex_lock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "User_db_delete_user mutex failed to lock");
        return false;
    }

    bool is_admin = false;
    int admin_idx = -1;
    for (size_t idx = 0; idx < user_database->user_count; idx++)
    {
        if (user_database->users[idx].session_id == admin_session_id)
        {
            is_admin = user_database->users[idx].is_admin;
            admin_idx = (int)idx;
            break;
        }
    }

    if (!is_admin || 0 > admin_idx)
    {
        syslog_write(log_file, ERROR, "Delete user request not from admin or invalid session");
        goto cleanup;
    }

    int user_idx = -1;
    for (size_t jdx = 0; jdx < user_database->user_count; jdx++)
    {
        if (STRNCMP_MATCH == strncmp(user_database->users[jdx].username, username, USERNAME_MAX_LEN))
        {
            user_idx = (int)jdx;
            break;
        }
    }

    if (0 > user_idx)
    {
        syslog_write(log_file, ERROR, "User to delete not found");
        goto cleanup;
    }

    if (user_idx == admin_idx)
    {
        syslog_write(log_file, ERROR, "Admin cannot delete their own account");
        goto cleanup;
    }
    
    // Why: We first check to see if the user we're deleting is the last
    // in the array, if so just zero the user out, and decrement the user count.
    // If it isn't the last one we just replace the user to delete with the last
    // user in array and then zero out the space of the moved user, and decrement
    // the count.
    if (user_idx == (int)(user_database->user_count - 1)) 
    {
        user_t *user_to_delete = &user_database->users[user_idx];
        user_to_delete->username[0] = '\0';
        user_to_delete->password[0] = '\0';
        user_to_delete->is_admin = false;
        user_to_delete->session_id = 0;
        user_to_delete->session_creation_time = 0;
    } 
    else 
    {
        user_database->users[user_idx] = user_database->users[user_database->user_count - 1];
        
        user_t *last_user = &user_database->users[user_database->user_count - 1];
        last_user->username[0] = '\0';
        last_user->password[0] = '\0';
        last_user->is_admin = false;
        last_user->session_id = 0;
        last_user->session_creation_time = 0;
    }

    user_database->user_count--;

    syslog_write(log_file, USER, "User deleted and last user in array moved");

    if (0 != pthread_mutex_unlock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "User_db_delete_user mutex failed to unlock");
        return false;
    }

    return true;

cleanup:
    if (0 != pthread_mutex_unlock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "User_db_delete_user mutex failed to unlock");
        return false;
    }
    return false;
}

bool user_db_login(user_db_t * user_database, const char *username, const char* password, uint32_t *session_id_out)
{
    if (NULL == user_database || NULL == username || NULL == password)
    {
        syslog_write(log_file, ERROR, "Login parameters invalid");
        return false;
    }

    if (0 != pthread_mutex_lock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "user_db_login mutex failed to lock");
        return false;
    }
    
    int user_idx = -1;
    for (size_t idx = 0; idx < user_database->user_count; idx++)
    {
        if (STRNCMP_MATCH == strncmp(user_database->users[idx].username, username, USERNAME_MAX_LEN))
        {
            user_idx = (int)idx;
            break;
        }
    }
    
    if (0 > user_idx)
    {
        syslog_write(log_file, ERROR, "Login failed: user not found");
        goto cleanup;
    }

    if (STRNCMP_MATCH != strncmp(user_database->users[user_idx].password, password, PASSWORD_MAX_LEN))
    {
        syslog_write(log_file, ERROR, "Login failed: incorrect password");
        goto cleanup;
    }

    uint32_t new_session_id = generate_new_session_id(user_database);

    user_database->users[user_idx].session_id = new_session_id;
    user_database->users[user_idx].session_creation_time = time(NULL);

    *session_id_out = new_session_id;
    
    syslog_write(log_file, LOGIN, "User login successful");
    
    if (0 != pthread_mutex_unlock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "user_db_login mutex failed to unlock");
        return false;
    }
    
    return true;

cleanup:
    if (0 != pthread_mutex_unlock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "user_db_login mutex failed to lock");
        return false;
    }

    return false;
}

bool user_db_logout(user_db_t *user_database, uint32_t session_id)
{
    if (NULL == user_database || 0 == session_id)
    {
        syslog_write(log_file, ERROR, "Logout parameters invalid");
        return false;
    }

    if (0 != pthread_mutex_lock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "Logout mutex failed to lock");
        return false;
    }

    bool session_id_found = false;
    
    for (size_t idx = 0; idx < user_database->user_count; idx++)
    {
        if (user_database->users[idx].session_id == session_id)
        {
            user_database->users[idx].session_id = 0;
            user_database->users[idx].session_creation_time = 0;
            session_id_found = true;
            
            syslog_write(log_file, LOGIN, "User logged out successfully");
            break;
        }
    }

    if (!session_id_found)
    {
        syslog_write(log_file, ERROR, "Logout failed: session not found");
    }

    if (0 != pthread_mutex_unlock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "Logout mutex failed to unlock");
        return false;
    }

    return true;
}

bool user_db_is_logged_in(user_db_t *user_database, uint32_t session_id)
{
    if (NULL == user_database || 0 == session_id)
    {
        syslog_write(log_file, ERROR, "Session validation parameters invalid");
        return false;
    }

    if (0 != pthread_mutex_lock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "Session validation mutex failed to lock");
        return false;
    }
    
    for (size_t idx = 0; idx < user_database->user_count; idx++)
    {
        if (user_database->users[idx].session_id == session_id)
        {
           break;
        }
        else
        {
            if (0 != pthread_mutex_unlock(&user_database->db_lock))
            {
                syslog_write(log_file, ERROR, "Session validation mutex failed to unlock");
                return false;
            }

            return false;
        }
    }

    if (0 != pthread_mutex_unlock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "Session validation mutex failed to unlock");
        return false;
    }

    return true;
}

void *user_db_session_monitor(void *arg)
{
    user_db_t *user_database = (user_db_t *)arg;
    if (NULL == user_database)
    {
        syslog_write(log_file, ERROR, "Session monitor invalid db pointer");
        return NULL;
    }
    
    for(;;)
    {

        sleep(SESSION_CHECK_SEC_INTERVAL);
        
        if (0 != pthread_mutex_lock(&user_database->db_lock))
        {
            syslog_write(log_file, ERROR, "Session monitor mutex failed to lock");
            continue;
        }
        
        time_t current_time = time(NULL);
        
        for (size_t idx = 0; idx < user_database->user_count; idx++)
        {
            if (0 != user_database->users[idx].session_id)
            {
                if (SESSION_TIMEOUT < (current_time - user_database->users[idx].last_activity_time))
                {
                    syslog_write(log_file, INFO, "User timeout reached");
                    user_database->users[idx].session_id = 0;
                }
            }
        }
        
        if (0 != pthread_mutex_unlock(&user_database->db_lock))
        {
            syslog_write(log_file, ERROR, "Session monitor mutex failed to unlock");
        }
    }

    return NULL;
}

bool user_db_update_activity(user_db_t *user_database, uint32_t session_id)
{
    if (NULL == user_database || 0 == session_id)
    {
        syslog_write(log_file, ERROR, "Update activity parameters invalid");
        return false;
    }

    if (0 != pthread_mutex_lock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "Update activity mutex failed to lock");
        return false;
    }
    
    for (size_t idx = 0; idx < user_database->user_count; idx++)
    {
        if (user_database->users[idx].session_id == session_id)
        {
            user_database->users[idx].last_activity_time = time(NULL);
            if (0 != pthread_mutex_unlock(&user_database->db_lock))
            {
                syslog_write(log_file, ERROR, "Update activity mutex failed to unlock");
                return false;
            }

            return true;
        }
    }

    if (0 != pthread_mutex_unlock(&user_database->db_lock))
    {
        syslog_write(log_file, ERROR, "Update activity mutex failed to unlock");
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

bool user_db_cleanup(user_db_t *user_database)
{
    if (NULL == user_database)
    {
        syslog_write(log_file, ERROR, "User database cleanup parameter invalid");
        return false;
    }

    if (NULL != user_database->users)
    {
        free(user_database->users);
        user_database->users = NULL;
    }

    int result = pthread_mutex_destroy(&user_database->db_lock);
    if (0 != result)
    {
        syslog_write(log_file, ERROR, "User database mutex destroy failed");
        return false;
    }

    user_database->user_count = 0;
    syslog_write(log_file, INFO, "User database cleaned up successfully");
    
    return true;
}

