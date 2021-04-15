/*
This file contains common definitions that are used throughout the program
*/

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>

//if defined, the EDD scheduler will be used. Otherwise, the vanilla FreeRTOS scheduler will be used.
//by default, the FreeRTOS scheduler will use round robin for tasks of the same priority
//and preemption for tasks of different priorities
#define EDD_ENABLED

typedef uint32_t time_t;
typedef uint8_t bool;//TODO: get rid of this if c++ support added

typedef enum {
    HW_EVT_TIMER,
    NUM_TIMERS
}timer_id_t;

typedef enum {
    HW_NEXT_EVENT,
	HW_IMU_REQUEST,
	HW_PPG_REQUEST,
    HW_SCREEN_UPDATE,
    HW_NUM_MESSAGES,
    NUM_MESSAGES
} message_type_t;

typedef struct edd_task {
	time_t deadline;
	bool periodic;
	void (*task_func)(void*);
    xTaskHandle task;
} edd_task_t;

typedef struct task_message_struct {
	message_type_t type;
    edd_task_t sender;
    void* data;
} task_msg_t;

#endif