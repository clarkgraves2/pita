#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#include "cmd_line_opts.h"
#include "user_db.h"
#include "syslog.h"

#define USERNAME_MAX_LEN (64)
#define DAYS_IN_YEAR (365)
#define SECONDS_PER_DAY (86400L) 
#define HOUR_DIV (100)
#define MIDNIGHT (2400)
#define LOG_MSG_BUFFER (100)
#define YEAR_BASE (1900)
#define YEAR_MIN (2000)
#define YEAR_MAX (3000)
#define MONTH_MAX (12)
#define DAY_MAX (31)
#define DATE_YEAR_FACTOR (10000)
#define DATE_MONTH_FACTOR (100)
#define NOON_HOUR (12)

typedef struct 
{
    char username[USERNAME_MAX_LEN];
    uint32_t session_id;
} reservation_info_t;

typedef struct 
{
    int hour;
    reservation_info_t * tables;
    int num_of_tables;
    int tables_available;
} time_slot_t;

typedef struct 
{
    int date;
    time_slot_t * hour_time_slots;
    int num_of_slots;
    int time_slots_available;
} date_slot_t;

typedef struct 
{
    date_slot_t * year_of_dates;
    int num_of_tables;
    int opening_hour;
    int closing_hour;
    int time_slots_per_day;
    pthread_mutex_t reserve_lock;
} reservation_system_t;

static FILE* log_file = NULL;
static reservation_system_t* g_reservation_system = NULL;
static user_db_t* g_user_database = NULL;

static int find_date_index(int date);
static int get_yyyymmdd_format(int day_index);

typedef struct 
{
    int date;
    int time;
} reservation_time_t;

bool reserv_sys_make_reservation(uint32_t session_id, reservation_time_t reservation_time);

static int calculate_num_time_slots(int opening_hour, int closing_hour)
{
    if (closing_hour == 0)
    {
        closing_hour = MIDNIGHT;
    }
    
    return (closing_hour - opening_hour) / HOUR_DIV;
}

static int find_date_index(int date)
{
    if (0 >= date)
    {
        syslog_write(log_file, ERROR, "Invalid date format for finding index");
        return -1;
    }

    time_t current_time = time(NULL);
    if ((time_t)-1 == current_time)
    {
        syslog_write(log_file, ERROR, "Failed to get current time");
        return -1;
    }

    struct tm *current_tm = localtime(&current_time);
    if (NULL == current_tm)
    {
        syslog_write(log_file, ERROR, "Failed to convert time to local time");
        return -1;
    }

    int current_date = (current_tm->tm_year + YEAR_BASE) * DATE_YEAR_FACTOR + 
                       (current_tm->tm_mon + 1) * DATE_MONTH_FACTOR + 
                       current_tm->tm_mday;

    if (date < current_date)
    {
        syslog_write(log_file, ERROR, "Date is in the past");
        return -1;
    }

    int year = date / DATE_YEAR_FACTOR;
    int month = (date / DATE_MONTH_FACTOR) % DATE_MONTH_FACTOR;
    int day = date % DATE_MONTH_FACTOR;

    struct tm target_tm = {0};
    target_tm.tm_year = year - YEAR_BASE;
    target_tm.tm_mon = month - 1;
    target_tm.tm_mday = day;
    target_tm.tm_hour = NOON_HOUR; 
    

    time_t target_time = mktime(&target_tm);
    if ((time_t)-1 == target_time)
    {
        syslog_write(log_file, ERROR, "Invalid date for conversion");
        return -1;
    }

    int day_diff = (int)((target_time - current_time) / SECONDS_PER_DAY);
    
    if (0 > day_diff || DAYS_IN_YEAR <= day_diff)
    {
        syslog_write(log_file, ERROR, "Date out of valid range");
        return -1;
    }

    return day_diff;
}

static int get_yyyymmdd_format(int day_index)
{
    if (0 > day_index || DAYS_IN_YEAR <= day_index)
    {
        syslog_write(log_file, ERROR, "Invalid day index for date conversion");
        return 0;
    }

    time_t current_time = time(NULL);
    if ((time_t)-1 == current_time)
    {
        syslog_write(log_file, ERROR, "Failed to get current time");
        return 0;
    }

    time_t future_time = current_time + ((long)day_index * SECONDS_PER_DAY);
    
    struct tm *time_info = localtime(&future_time);
    if (NULL == time_info)
    {
        syslog_write(log_file, ERROR, "Failed to convert time to local time");
        return 0;
    }

    int date = (time_info->tm_year + YEAR_BASE) * DATE_YEAR_FACTOR + 
               (time_info->tm_mon + 1) * DATE_MONTH_FACTOR + 
               time_info->tm_mday;
    
    return date;
}

static date_slot_t *find_date_slot(int date_yyyymmdd)
{
    if (NULL == g_reservation_system)
    {
        syslog_write(log_file, ERROR, "Reservation system not initialized");
        return NULL;
    }

    int date_index = find_date_index(date_yyyymmdd);
    if (0 > date_index || DAYS_IN_YEAR <= date_index)
    {
        syslog_write(log_file, ERROR, "Date index out of range");
        return NULL;
    }

    return &g_reservation_system->year_of_dates[date_index];
}

static time_slot_t *find_time_slot(date_slot_t *date_slot, int time)
{
    if (NULL == date_slot || NULL == date_slot->hour_time_slots)
    {
        syslog_write(log_file, ERROR, "Invalid date slot for finding time");
        return NULL;
    }

    int slot_index = (time - g_reservation_system->opening_hour) / HOUR_DIV;
    
    if (slot_index < 0 || slot_index >= date_slot->num_of_slots)
    {
        syslog_write(log_file, ERROR, "Time slot index out of range");
        return NULL;
    }

    return &date_slot->hour_time_slots[slot_index];
}

bool is_date_valid(int date)
{
    if (0 >= date)
    {
        return false;
    }

    int year = date / DATE_YEAR_FACTOR;
    int month = (date / DATE_MONTH_FACTOR) % DATE_MONTH_FACTOR;
    int day = date % DATE_MONTH_FACTOR;
    
    if (YEAR_MIN > year || YEAR_MAX < year || 1 > month || MONTH_MAX < month || 1 > day || DAY_MAX < day)
    {
        return false;
    }
    
    return (0 <= find_date_index(date));
}

void reserv_sys_cleanup(void) 
{
    if (NULL == g_reservation_system) 
    {
        return;
    }
    
    if (NULL != g_reservation_system->year_of_dates) 
    {
        for (int idx = 0; idx < DAYS_IN_YEAR; idx++) 
        {
            if (NULL != g_reservation_system->year_of_dates[idx].hour_time_slots) 
            {
                for (int jdx = 0; jdx < g_reservation_system->time_slots_per_day; jdx++) 
                {
                    if (NULL != g_reservation_system->year_of_dates[idx].hour_time_slots[jdx].tables) 
                    {
                        free(g_reservation_system->year_of_dates[idx].hour_time_slots[jdx].tables);
                    }
                }
                free(g_reservation_system->year_of_dates[idx].hour_time_slots);
            }
        }
        free(g_reservation_system->year_of_dates);
    }
    
    pthread_mutex_destroy(&g_reservation_system->reserve_lock);
    
    free(g_reservation_system);
    g_reservation_system = NULL;
}

bool reserv_sys_init(user_db_t * user_database, cmd_line_options_t * userdb_configs,
    const volatile sig_atomic_t *serv_running) 
{
    if (NULL == user_database || NULL == userdb_configs || NULL == serv_running)
    {
        syslog_write(userdb_configs->log_file, ERROR, "Init parameters fo reserv system failed");
        return false;
    }

    log_file = userdb_configs->log_file;
    g_user_database = user_database;

    g_reservation_system = calloc(1, sizeof(reservation_system_t));
    if (NULL == g_reservation_system)
    {
        syslog_write(log_file, ERROR, "Reservation system failed to allocate");
        return false;
    }

    g_reservation_system->num_of_tables = userdb_configs->num_tables;
    g_reservation_system->opening_hour = userdb_configs->opening_hour;
    g_reservation_system->closing_hour = userdb_configs->closing_hour;
    g_reservation_system->time_slots_per_day = calculate_num_time_slots(
        g_reservation_system->opening_hour, 
        g_reservation_system->closing_hour);
        
    if (0 != pthread_mutex_init(&g_reservation_system->reserve_lock, NULL))
    {
        syslog_write(log_file, ERROR, "Failed to initialize reservation system mutex");
        free(g_reservation_system);
        g_reservation_system = NULL;
        return false;
    }

    g_reservation_system->year_of_dates = calloc(DAYS_IN_YEAR, sizeof(date_slot_t));
    if (NULL == g_reservation_system->year_of_dates)
    {
        syslog_write(log_file, ERROR, "Failed to allocate memory for year of dates");
        pthread_mutex_destroy(&g_reservation_system->reserve_lock);
        free(g_reservation_system);
        g_reservation_system = NULL;
        return false;
    }

    for (int date_idx = 0; date_idx < DAYS_IN_YEAR; date_idx++)
    {
        int date_in_yyyymmdd = get_yyyymmdd_format(date_idx);
        if (0 == date_in_yyyymmdd)
        {
            syslog_write(log_file, ERROR, "Failed to calculate date in YYYYMMDD format");
            reserv_sys_cleanup();
            return false;
        }
        
        g_reservation_system->year_of_dates[date_idx].date = date_in_yyyymmdd;
        g_reservation_system->year_of_dates[date_idx].num_of_slots = g_reservation_system->time_slots_per_day;
        g_reservation_system->year_of_dates[date_idx].time_slots_available = g_reservation_system->time_slots_per_day;
        
        g_reservation_system->year_of_dates[date_idx].hour_time_slots = 
            calloc(g_reservation_system->time_slots_per_day, sizeof(time_slot_t));
        
        if (NULL == g_reservation_system->year_of_dates[date_idx].hour_time_slots)
        {
            syslog_write(log_file, ERROR, "Failed to allocate memory for time slots");
            reserv_sys_cleanup();
            return false;
        }

        for (int slot_idx = 0; slot_idx < g_reservation_system->time_slots_per_day; slot_idx++)
        {
            time_slot_t *current_slot = &g_reservation_system->year_of_dates[date_idx].hour_time_slots[slot_idx];
            
            current_slot->hour = g_reservation_system->opening_hour + (slot_idx * HOUR_DIV);
            current_slot->num_of_tables = g_reservation_system->num_of_tables;
            current_slot->tables_available = g_reservation_system->num_of_tables;
            
            current_slot->tables = calloc(g_reservation_system->num_of_tables, sizeof(reservation_info_t));
            if (NULL == current_slot->tables)
            {
                syslog_write(log_file, ERROR, "Failed to allocate memory for tables");
                reserv_sys_cleanup();
                return false;
            }
            
            for (int table_idx = 0; table_idx < g_reservation_system->num_of_tables; table_idx++)
            {
                current_slot->tables[table_idx].session_id = 0;
                current_slot->tables[table_idx].username[0] = '\0';
            }
        }
    }

    syslog_write(log_file, INFO, "Reservation system initialized successfully");
    return true;
}

bool reserv_sys_make_reservation(uint32_t session_id, reservation_time_t reservation_time)
{
    if (NULL == g_reservation_system || NULL == g_user_database)
    {
        syslog_write(log_file, ERROR, "Reservation system not initialized");
        return false;
    }

    if (0 == session_id)
    {
        syslog_write(log_file, ERROR, "Invalid session ID for reservation");
        return false;
    }

    char username[USERNAME_MAX_LEN];
    if (!user_db_get_username(g_user_database, session_id, username, USERNAME_MAX_LEN))
    {
        syslog_write(log_file, ERROR, "Failed to get username for session");
        return false;
    }

    if (0 != pthread_mutex_lock(&g_reservation_system->reserve_lock))
    {
        syslog_write(log_file, ERROR, "Failed to lock reservation mutex");
        return false;
    }

    bool result = false;
    date_slot_t *date_slot = find_date_slot(reservation_time.date);
    if (NULL == date_slot)
    {
        syslog_write(log_file, ERROR, "Invalid date for reservation");
        goto cleanup;
    }

    time_slot_t *time_slot = find_time_slot(date_slot, reservation_time.time);
    if (NULL == time_slot)
    {
        syslog_write(log_file, ERROR, "Invalid time for reservation");
        goto cleanup;
    }

    if (0 >= time_slot->tables_available)
    {
        syslog_write(log_file, ERROR, "No tables available for reservation");
        goto cleanup;
    }

    for (int idx = 0; idx < time_slot->num_of_tables; idx++)
    {
        if (session_id == time_slot->tables[idx].session_id)
        {
            syslog_write(log_file, ERROR, "User already has a reservation at this time");
            goto cleanup;
        }
    }

    for (int jdx = 0; jdx < time_slot->num_of_tables; jdx++)
    {
        if (0 == time_slot->tables[jdx].session_id)
        {
            time_slot->tables[jdx].session_id = session_id;
            strncpy(time_slot->tables[jdx].username, username, USERNAME_MAX_LEN - 1);
            time_slot->tables[jdx].username[USERNAME_MAX_LEN - 1] = '\0';
            
            time_slot->tables_available--;
            date_slot->time_slots_available = 
                (0 == time_slot->tables_available) ? 
                date_slot->time_slots_available - 1 : 
                date_slot->time_slots_available;

            char log_msg[LOG_MSG_BUFFER];
            snprintf(log_msg, sizeof(log_msg), 
                    "Reservation made: user=%s, date=%d, time=%d, table=%d", 
                    username, reservation_time.date, reservation_time.time, jdx);
            syslog_write(log_file, INFO, log_msg);
            
            result = true;
            break;
        }
    }

cleanup:
    if (0 != pthread_mutex_unlock(&g_reservation_system->reserve_lock))
    {
        syslog_write(log_file, ERROR, "Failed to unlock reservation mutex");
        return false;
    }
    
    return result;
}