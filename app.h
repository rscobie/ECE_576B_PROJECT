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
	uint32_t event_data;
} hw_evt_t;

typedef uint32_t ppg_sample_t; //time since last peak
typedef uint32_t imu_sample_t; //magnitude of acceleration vector

#define NUM_EVENTS 10
#define NUM_PPG_SAMPLES 100
#define NUM_IMU_SAMPLES 100

#define APERIODIC_PERIOD 0

void hw_timer_callback(TimerHandle_t xTimer);
void hardware_task(void* pvParameters);
void init_app();
void generator_task(void* pvParameters);
void activity_task(void* pvParameters);
void hr_monitor_task(void* pvParameters);
void app_task(void* pvParameters);
void ui_task(void* pvParameters);

xTaskHandle hw_task_handle;

/*
 * generator task definitions
 */
xTaskHandle generator_task_handle;

/*
 * activity task definitions
 */
#define ACT_TASK_PERIOD 100
edd_task_t act_task_data;

/*
 * heart rate task definitions
 */
#define HRM_TASK_PERIOD 100
edd_task_t hrm_task_data;

/*
 * application task definitions
 */
#define APP_TASK_PERIOD APERIODIC_PERIOD
#define APP_TASK_RELATIVE_DEADLINE 10
edd_task_t app_task_data;

/*
 * user interface task definitions
 */
#define UI_TASK_PERIOD APERIODIC_PERIOD
#define UI_TASK_RELATIVE_DEADLINE 5
edd_task_t ui_task_data;

#endif
