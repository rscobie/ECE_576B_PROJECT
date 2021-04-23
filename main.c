//extern "C" {
#include "edd.h"
#include "app.h"
#include "common.h"
//}
/******************************************
 * 				definitions
 *****************************************/

/******************************************
 * 				variables
 *****************************************/

/******************************************
 * 				functions
 *****************************************/

int main(){
    init_app(); //initializes necessary data structures for app
    #ifdef EDD_ENABLED
    init_scheduler();//initializes necessary data structures for scheduler
    xTaskCreate(monitor_task, "monitor", configMINIMAL_STACK_SIZE, NULL, MONITOR_TASK_PRIORITY, monitor_task_handle);
    xTaskCreate(scheduler_task, "scheduler", TASK_STACK_SIZE, NULL, EDD_PRIORITY, scheduler_task_handle);
    #endif
    xTaskCreate(hardware_task, "hardware", TASK_STACK_SIZE, NULL, HW_PRIORITY, hw_task_handle);
    xTaskCreate(generator_task, "generator", TASK_STACK_SIZE, NULL, GENERATOR_PRIORITY, generator_task_handle);
    vTaskStartScheduler();
    return 0;
}