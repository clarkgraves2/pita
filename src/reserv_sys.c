
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>

#include "cmd_line_opts.h"
#include "user_db.h"
#include "syslog.h"

#define USERNAME_MAX_LEN (64)

typedef struct 
{
    char username[USERNAME_MAX_LEN];
    uint32_t session_id;
}reserved_entry_t;

typedef struct 
{
    int hour;
    reserved_entry_t * tables;
    int num_of_tables;
    int tables_available;
}time_slot_t;

typedef struct 
{
    int date;
    time_slot_t * hour_time_slots;
    int num_of_slots;
    int time_slots_available;
}date_slot_t;

typedef struct 
{
    date_slot_t * year_of_dates;
    int num_of_tables;
    int opening_hour;
    int closing_hour;
    int time_slots_per_day;
    pthread_mutex_t reserve_lock;
}reservation_system_t;

bool reserv_sys_init(user_db_t * user_database, cmd_line_options_t * userdb_configs,
    volatile sig_atomic_t *serv_running)
{
    
}