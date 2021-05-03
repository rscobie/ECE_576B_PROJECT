#include "edd.h"
#include <limits.h>

//performance metrics
static int max_lateness = INT_MIN;
static int num_late_tasks =  0;

static edd_task_t* task_priority_queue[NUM_APP_TASKS];

void init_scheduler(){
    scheduler_queue_handle = xQueueCreate(MSG_QUEUE_SIZE, sizeof(task_msg_t));
    //init priority queue
    for(int i = 0; i < NUM_APP_TASKS; ++i){
        task_priority_queue[i] = NULL; //no function, indicates inactive
    }
}

void scheduler_task(void* pvParameters){
    task_msg_t message = {};
    while(1){
        //printf("scheduler task\n");
        xQueueReceive(scheduler_queue_handle, &message, portMAX_DELAY); //Yield until message
        //printf("%s\n", message.data);
        switch (message.type) {
            case EDD_TASK_CREATE: 
                //printf("task create\n");
                // call deadline_insertion
                deadline_insertion((edd_task_t*)message.sender);
                break;
            case EDD_TASK_PERIODIC_DELAY: //sent by periodic task whenever it waits until next period
                //printf("task periodic delay\n");
                
                // Update task_priority_queue => call deadline_update
                deadline_removal((edd_task_t*)message.sender);
                deadline_insertion((edd_task_t*)message.sender); 
                // Resume sender task 
                break;
            case EDD_TASK_PERIODIC_SUSPEND://send by application task (e.g. disable HRM)
                //printf("task periodic suspend\n");
                
                // Suspend desired task (handle included in message)
                deadline_removal((edd_task_t*)message.sender);//TODO: rename since not actually sender
                // Remove desired task from priority queue
                break;
            case EDD_TASK_APERIODIC_SUSPEND://sent from aperiodic task
                //printf("task aperiodic suspend\n");
                
                // Suspend sender task
                deadline_removal((edd_task_t*)message.sender);//TODO: rename since not actually sender
                // Remove sender task from priority queue
                break;
            case EDD_TASK_PERIODIC_RESUME://sent by application task
                //printf("task periodic resume\n");
                // Assign deadline based on period
                
                // Add desired task to priority queue
                deadline_insertion((edd_task_t*)message.sender);//TODO: rename since not actually sender
                // Resume desired task
                break;
            case EDD_TASK_APERIODIC_RESUME://sent by hardware task
                //printf("task aperiodic resume\n");
                // Assign deadline based on period
                
                // Add desired task to priority queue
                deadline_insertion((edd_task_t*)message.sender);//TODO: rename since not actually sender
                // Resume desired task
                break;
            case EDD_TASK_DELETE:
                //printf("task delete delay\n");
                // call deadline_removal //probably won’t need
                deadline_removal((edd_task_t*)message.data);
                break;
        }
        //update priorities here
        for(int i = 0; i < NUM_APP_TASKS; ++i){
            if(task_priority_queue[i] == NULL){
                break;
            }
            //printf("updating priorities\n");
            vTaskPrioritySet(*(task_priority_queue[i]->task), BASE_APP_PRIORITY - i);
        }
        // send reply to messenger
        //TODO: I think we can skip reply? 
    }
}

//scheduler helper functions 
void deadline_insertion( edd_task_t* sender ){
	//Insert task_handler into the vector task_priority_queue //first deadline is first element
    int index = 0;
    for(int i = 0; i < NUM_APP_TASKS; ++i){
        if(task_priority_queue[i] == NULL || task_priority_queue[i]->deadline > sender->deadline){
            index = i;
            break;
        }
    }
    edd_task_t* temp;
    temp = task_priority_queue[index];
    task_priority_queue[index] = sender;
    for(int i = index+1; i < NUM_APP_TASKS; ++i){
        task_priority_queue[i] = temp;
        if(i == NUM_APP_TASKS - 1){
            break;
        }
        temp = task_priority_queue[i+1];
    }
}

void deadline_removal(edd_task_t* sender ){
	//Locate the task and Delete the task_handler from the vector,
    int index = 0;
    for(int i = 0; i < NUM_APP_TASKS; ++i){
        if(task_priority_queue[i] == NULL || task_priority_queue[i]->task == sender->task){
            index = i;
            break;
        }
    }
    for(int i = index; i < NUM_APP_TASKS; ++i){
        if(i == NUM_APP_TASKS - 1){
            task_priority_queue[i] = NULL; // indicates inactive
            break;
        }
        task_priority_queue[i] = task_priority_queue[i+1];
    }
}

void task_delay(edd_task_t* sender){
    time_t lateness = INT_MIN;
    if(sender->first_time){
        sender->first_time = false;
        lateness = 1;//just to get use to add period to the current time, doesn't effect max lateness
    }
    else{
        lateness = xTaskGetTickCount() - sender->deadline;
        if(lateness > max_lateness){
            printf("new max lateness: %d\n", lateness);
            max_lateness = lateness;
        }
        if(lateness > 0){
            num_late_tasks++;
        }
    }
    // Recalculate deadline based on period (add period to deadline)
    if(lateness <= 0){
        sender->deadline += sender->period;
    }
    else{ //if we missed deadline, add to current time //TODO: is this OK to do?
        sender->deadline = xTaskGetTickCount() + sender->period;
    }
    #ifdef EDD_ENABLED
    task_msg_t message = {};
    message.type = EDD_TASK_PERIODIC_DELAY;
    message.sender = sender;
    xQueueSend(scheduler_queue_handle, &message, portMAX_DELAY);
    #endif
    if(lateness <= 0){//wait for the rest of the period (until old deadline)
        vTaskDelay(-1*lateness);
    }
    else{//since we were late, go again immediately
        //intentionally left empty
    }
}

void task_suspend(edd_task_t* sender){ //TODO: should rename since can be called from different task
    time_t lateness = INT_MIN;
    lateness = xTaskGetTickCount() - sender->deadline;
    if(lateness > max_lateness){
        printf("new max lateness: %d\n", lateness);
        max_lateness = lateness;
    }
    if(lateness > 0){
        num_late_tasks++;
    }
    #ifdef EDD_ENABLED
    task_msg_t message = {};
    if(sender->periodic){
        message.type = EDD_TASK_PERIODIC_SUSPEND;
    }
    else{
        message.type = EDD_TASK_APERIODIC_SUSPEND;
    }
    message.sender = sender;
    xQueueSend(scheduler_queue_handle, &message, portMAX_DELAY);
    #endif
    vTaskSuspend(*sender->task);
}

void task_resume(edd_task_t* sender){//TODO: should rename since can be called from different task
    if(sender->periodic){
        sender->deadline = xTaskGetTickCount() + sender->period;
    }
    else{
        sender->deadline = xTaskGetTickCount() + sender->relative_deadline;
    }
    #ifdef EDD_ENABLED
    task_msg_t message = {};
    if(sender->periodic){
        message.type = EDD_TASK_PERIODIC_RESUME;
    }
    else{
        message.type = EDD_TASK_APERIODIC_RESUME;
    }
    message.sender = sender;
    xQueueSend(scheduler_queue_handle, &message, portMAX_DELAY);
    #endif
    vTaskResume(*sender->task);
}

void task_wait_for_evt(edd_task_t* sender){ //TODO: should rename since can be called from different task
    time_t lateness = INT_MIN;
    if(sender->first_time){
        sender->first_time = false;
    }
    else{
        lateness = xTaskGetTickCount() - sender->deadline;
        if(lateness > max_lateness){
            printf("new max lateness: %d\n", lateness);
            max_lateness = lateness;
        }
        if(lateness > 0){
            num_late_tasks++;
        }
    }
    #ifdef EDD_ENABLED
    task_msg_t message = {};
    if(sender->periodic){
        message.type = EDD_TASK_PERIODIC_SUSPEND;
    }
    else{
        message.type = EDD_TASK_APERIODIC_SUSPEND;
    }
    message.sender = sender;
    xQueueSend(scheduler_queue_handle, &message, portMAX_DELAY);
    #endif
}

void task_got_evt(edd_task_t* sender){//TODO: should rename since can be called from different task
    if(sender->periodic){
        sender->deadline = xTaskGetTickCount() + sender->period;
    }
    else{
        sender->deadline = xTaskGetTickCount() + sender->relative_deadline;
    }
    #ifdef EDD_ENABLED
    task_msg_t message = {};
    if(sender->periodic){
        message.type = EDD_TASK_PERIODIC_RESUME;
    }
    else{
        message.type = EDD_TASK_APERIODIC_RESUME;
    }
    message.sender = sender;
    xQueueSend(scheduler_queue_handle, &message, portMAX_DELAY);
    #endif
}

void task_create(edd_task_t* sender, char* name, priority_t priority){
    //printf("%s\n", name);
    xTaskCreate(sender->task_func, name, TASK_STACK_SIZE, NULL, priority, sender->task );
    #ifdef EDD_ENABLED
    vTaskSuspend(*sender->task);
	// Create message to send to scheduler that a task is created
    task_msg_t message = {};
    message.type = EDD_TASK_CREATE;
    message.sender = sender;
    // Send to scheduler
    xQueueSend(scheduler_queue_handle, &message, portMAX_DELAY);
    //wait for message back from scheduler
    //TODO: I think we can skip waiting for reply?
    #endif
    vTaskResume(*sender->task);
}

void task_delete(edd_task_t* sender){
    #ifdef EDD_ENABLED
	// Create message to scheduler that a task handler will be deleted
	task_msg_t message = {};
    message.type = EDD_TASK_DELETE;
    message.sender = sender;
    // Send to scheduler
    xQueueSend(scheduler_queue_handle, &message, portMAX_DELAY);
	// Wait til scheduler replies
    //TODO: I think we can skip waiting for reply?
    #endif
	vTaskDelete ( *sender->task );
}

void monitor_task(void* pvParameters)
{
    const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
    time_t current_time = xTaskGetTickCount();

    for (;;)
    {
        //printf("monitor task\n");
        current_time = xTaskGetTickCount();

        if (current_time < MAX_TIME)
        {
            printf("Current time: %d ms\n", current_time);

            srand(current_time);

            vTaskDelay(xDelay);
        }
        else {
            printf("Application runtime has exceeded %d ms\n", MAX_TIME);
            printf("Current runtime %d ms\n", current_time);
            //vTaskDelete(xSADcalcHandle);
            //vTaskDelete(NULL);
            vTaskSuspendAll();
        }
    }
}