
/**
 * @file reserv_sys.h
 * @brief Header file for the Pita-bytes Restaurant Reservation System
 */

#ifndef RESERV_SYS_H
#define RESERV_SYS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "common.h"

typedef struct reservation_time 
{
    int date; 
    int time;  
} reservation_time_t;

reservation_system_t* reserv_sys_init(server_state_t* server_state);

bool reserv_sys_make_reservation(uint32_t session_id, reservation_time_t reservation_time);

bool reserv_sys_cancel_reservation(uint32_t session_id, reservation_time_t reservation_time);


bool reserv_sys_list_available(int date, char* result_buffer, size_t buffer_size);

bool reserv_sys_get_bookings(uint32_t session_id, const char* username, 
                            bool is_admin, char* result_buffer, size_t buffer_size);

bool is_date_valid(int date);

bool is_time_valid(int time, int opening_hour, int closing_hour);

void reserv_sys_cleanup(reservation_system_t* reservation_system);

#endif /* RESERV_SYS_H */