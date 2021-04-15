#ifndef APP_H
#define APP_H

#include "common.h"

/*
 * hardware task definitions
 */
typedef enum {
	HW_EVT_SHORT_PRESS,
	HW_EVT_LONG_PRESS,
} hw_evt_type_t;

typedef struct hw_evt {
	hw_evt_type_t event_type;
	time_t time_stamp;
	char* event_data;
} hw_evt_t;

#define NUM_EVENTS 10

void hw_timer_callback(TimerHandle_t xTimer);
void hardware_task (void* pvParameters);

/*
 * TODO: other task definitions
 */

#endif