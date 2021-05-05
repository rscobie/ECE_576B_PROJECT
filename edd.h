#ifndef EDD_H
#define EDD_H

#include "common.h"

#define MAX_TIME 1000000 //max time for program to run

xTaskHandle scheduler_task_handle;
xTaskHandle monitor_task_handle;
xQueueHandle scheduler_queue_handle;


void init_scheduler();
void task_create(edd_task_t* sender, char* name, priority_t priority);
void task_delete(edd_task_t* sender);
void task_delay(edd_task_t* sender);
void task_suspend(edd_task_t* sender);
void task_resume(edd_task_t* sender);
void deadline_insertion(edd_task_t* sender); //resume task
void deadline_removal(edd_task_t* sender); //suspend task
void scheduler_task(void* pvParameters);
void monitor_task(void* pvParameters);
void task_wait_for_evt(edd_task_t* sender);
void task_got_evt(edd_task_t* sender);


#endif