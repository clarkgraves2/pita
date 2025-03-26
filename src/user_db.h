#ifndef USER_DB_H
#define USER_DB_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h> 


#include "cmd_line_opts.h"
#include "syslog.h"

typedef struct user_db user_db_t;

#define USERNAME_MAX_LEN (64)
#define PASSWORD_MAX_LEN (128)

bool user_db_register(user_db_t * user_database, const char *username, const char* password, bool is_admin);

bool user_db_delete_user(user_db_t *user_database, const char *username, uint32_t admin_session_id);

bool user_db_login(user_db_t * user_database, const char *username, const char* password, uint32_t *session_id_out);

bool user_db_logout(user_db_t *user_database, uint32_t session_id);

bool user_db_is_logged_in(user_db_t *user_database, uint32_t session_id);

bool user_db_get_username(user_db_t *user_database, uint32_t session_id, 
                          char *username_out, size_t username_max);

void *user_db_session_monitor(void *arg);

bool user_db_update_activity(user_db_t *user_database, uint32_t session_id);

bool user_db_is_admin(user_db_t *user_database, uint32_t session_id);

bool user_db_init(user_db_t * user_database, cmd_line_options_t * userdb_configs,
                  volatile sig_atomic_t *serv_running);

bool user_db_cleanup(user_db_t *user_database);

#endif

/*** end of file ***/