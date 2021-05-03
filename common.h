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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//if defined, the EDD scheduler will be used. Otherwise, the vanilla FreeRTOS scheduler will be used.
//by default, the FreeRTOS scheduler will use round robin for tasks of the same priority
//and preemption for tasks of different priorities
#define EDD_ENABLED

//disable preemption for EDD, otherwise we'd be doing EDF
#ifdef EDD_ENABLED
#undef configUSE_PREEMPTION
#define configUSE_PREEMPTION					0
#endif

#define NUM_APP_TASKS 4

#define TASK_STACK_SIZE 1024 //bytes

//TODO: change this if necessary
#define MSG_DATA_SIZE 100 //max size in bytes of any data sent via message.

#define MSG_QUEUE_SIZE 100

//typedef uint32_t time_t;
#ifndef __cplusplus //don't include this block if we're compiling with C++
typedef uint8_t bool;
#define true 1
#define false 0
typedef uint8_t byte;
#endif

typedef enum {
    MONITOR_TASK_PRIORITY = configTIMER_TASK_PRIORITY - 1, //scheduler is just below timer
    EDD_PRIORITY = MONITOR_TASK_PRIORITY - 1,
    GENERATOR_PRIORITY = EDD_PRIORITY - 1,
    HW_PRIORITY = GENERATOR_PRIORITY - 1, //hardware is just below scheduler but above app
    BASE_APP_PRIORITY = HW_PRIORITY - 1 //highest app task priority
}priority_t;

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
    ACT_IMU_DATA,
    ACT_NUM_MESSAGES,
    HRM_PPG_DATA,
    HRM_NUM_MESSAGES,
    APP_NUM_MESSAGES,
    APP_REMINDER, 
    APP_HEARTRATE,
    APP_ACT_TYPE_UPDATE,
    UI_LONG_BUTTON_PRESS,
    UI_SHORT_BUTTON_PRESS,
    UI_NUM_MESSAGES,
    EDD_TASK_CREATE,
    EDD_TASK_PERIODIC_DELAY,
    EDD_TASK_PERIODIC_SUSPEND,
    EDD_TASK_APERIODIC_SUSPEND,
    EDD_TASK_PERIODIC_RESUME,
    EDD_TASK_APERIODIC_RESUME,
    EDD_TASK_DELETE,
    EDD_NUM_MESSAGES
} message_type_t;

typedef struct edd_task {
    time_t deadline;
    bool periodic;
    time_t period;
    bool first_time;
    time_t relative_deadline;
    void (*task_func)(void*);
    xTaskHandle* task;
} edd_task_t;

typedef struct task_message_struct {
    message_type_t type;
    edd_task_t* sender;
    byte data[MSG_DATA_SIZE];
} task_msg_t;

#endif
