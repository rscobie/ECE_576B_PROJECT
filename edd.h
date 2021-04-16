#ifndef EDD_H
#define EDD_H

#include "common.h"

xTaskHandle scheduler_task_handle;
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

#endif