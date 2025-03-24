#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define USERNAME_MAX_LEN (64)
#define PASSWORD_MAX_LEN (128)

typedef struct {
    char username[USERNAME_MAX_LEN];
    char pass_hash[PASSWORD_MAX_LEN]; 
    bool is_admin;
    uint32_t session_id;  
    time_t session_creation_time; 
} user_t;

typedef struct
{
    user_t * users;
    size_t user_capacity;
    size_t user_count;
    pthread_mutex_t db_lock;
}user_db_t;