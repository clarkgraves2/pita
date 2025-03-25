
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

#include "cmd_line_opts.h"
#include "user_db.h"
#include "syslog.h"

#define USERNAME_MAX_LEN (64)
#define DAYS_IN_YEAR (365)

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

FILE * log_file = NULL;

bool reserv_sys_init(user_db_t * user_database, cmd_line_options_t * userdb_configs,
    volatile sig_atomic_t *serv_running)
{
    if (NULL == user_database || NULL == userdb_configs || NULL == serv_running)
    {
        syslog_write(userdb_configs->log_file, ERROR, "Init parameters fo reserv system failed");
    }

    log_file = userdb_configs->log_file;

    reservation_system_t * reservation_system = calloc(1, sizeof(reservation_system_t));
    if (NULL == reservation_system)
    {
        syslog_write(log_file, ERROR, "Reservation system failed to allocate");
        free(log_file);
        log_file = NULL;
        return false;
    }

    reservation_system->year_of_dates = calloc(DAYS_IN_YEAR, sizeof(date_slot_t));
    if (NULL == reservation_system->year_of_dates)
    {
        syslog_write(log_file, ERROR, "Dates to reserve failed to allocate");
        free(reservation_system);
        reservation_system = NULL;
        free(log_file);
        log_file = NULL;
        return false;
    }

    reservation_system->num_of_tables = userdb_configs->num_tables;
    reservation_system->opening_hour = userdb_configs->opening_hour;
    reservation_system->closing_hour = userdb_configs->closing_hour;
    

}