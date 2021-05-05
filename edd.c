#include "edd.h"
#include <limits.h>

char late_file_name[] = "lateness.log";
FILE* late_file;
//performance metrics
static int max_lateness = INT_MIN;
static int num_late_tasks =  0;
#define COMPLETION_NUM_CYCLES 1000
static int completion_counter = 0; //will count out a certain number of task suspends, then save 'total completion time'
static int total_completion_time = 0;

//response times
static int avg_app_response_time = 0;
static int app_wait_time = 0;
static int avg_hrm_response_time = 0;
static int hrm_wait_time = 0;
static int avg_ui_response_time = 0;
static int ui_wait_time = 0;
static int avg_act_response_time = 0;
static int act_wait_time = 0;

char lateness_file_name[] = "lateness.log";
FILE* lateness_file;
char lateTask_file_name[] = "lateTask.log";
FILE* lateTask_file;



static edd_task_t* task_priority_queue[NUM_APP_TASKS];

void init_scheduler(){
    #ifdef EDD_ENABLED
    lateness_file = fopen(lateness_file_name, "w");
    lateTask_file = fopen(lateTask_file_name, "w");
    #endif
    scheduler_queue_handle = xQueueCreate(MSG_QUEUE_SIZE, sizeof(task_msg_t));
    //late_file = fopen(late_file_name, "a");
    //fprintf(late_file, "this");
    //fclose(late_file);
    //init priority queue
    for(int i = 0; i < NUM_APP_TASKS; ++i){
        task_priority_queue[i] = NULL; //no function, indicates inactive
    }
}

void scheduler_task(void* pvParameters){
    task_msg_t message = {};
    while(1){
        //printf("scheduler task\n");
        //printf("here 1111");
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
            //printf("here 2222");
            if(task_priority_queue[i] == NULL){
                break;
            }
            //printf("updating priorities\n");
            vTaskPrioritySet(*(task_priority_queue[i]->task), BASE_APP_PRIORITY - i);
        }
        // send reply to messenger
        //TODO: I think we can skip reply?
        
        //measure arrival time: TODO

        //make note of active task
    }
}

//scheduler helper functions 
void deadline_insertion( edd_task_t* sender ){
    //printf("here 3333");
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
   // printf("here 4444");
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
    //printf("here 5555");
    time_t lateness = INT_MIN;
    if(sender->first_time){
        sender->first_time = false;
        lateness = 1;//just to get use to add period to the current time, doesn't effect max lateness
    }
    else{
        #ifndef EDD_ENABLED
        lateness_file = fopen(lateness_file_name, "a");
        lateTask_file = fopen(lateTask_file_name, "a");
        #endif
        lateness = xTaskGetTickCount() - sender->deadline;
        if(lateness > max_lateness){
        printf("new max lateness: %d\n", lateness);
           // late_file = fopen(late_file_name, "a");
           // fprintf(late_file, "new max lateness: %d\n", (int)lateness);
            //fclose(late_file);
            max_lateness = lateness;
            fprintf(lateness_file, "%d\n", max_lateness);
        }
        if(lateness > 0){
            num_late_tasks++;
            fprintf(lateTask_file, "%d\n", num_late_tasks);
        }
        if(completion_counter < COMPLETION_NUM_CYCLES){
            completion_counter++;
        }
        else{
            total_completion_time = xTaskGetTickCount();
        }
    }
    // Recalculate deadline based on period (add period to deadline)
    if(lateness <= 0){
        sender->deadline += sender->period;
    }
    else{ //if we missed deadline, add to current time //TODO: is this OK to do?
        sender->deadline = xTaskGetTickCount() + sender->period;
    }
    
    //keep track of wait time
    // if(sender == &ui_task_data){

    // }

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

void task_suspend(edd_task_t* sender){ 
        time_t lateness = INT_MIN;
    //printf("here 6666");
    lateness = xTaskGetTickCount() - sender->deadline;
    #ifndef EDD_ENABLED
    lateness_file = fopen(lateness_file_name, "a");
    lateTask_file = fopen(lateTask_file_name, "a");
    #endif
    if(lateness > max_lateness){
     printf("new max lateness: %d\n", lateness);
       // late_file = fopen(late_file_name, "a");
         //   fprintf(late_file, "new max lateness: %d\n", (int)lateness);
         // fclose(late_file);
        max_lateness = lateness;
        fprintf(lateness_file, "%d\n", max_lateness);
    }
    if(lateness > 0){
        num_late_tasks++;
        fprintf(lateTask_file, "%d\n", num_late_tasks);
    }
    if(completion_counter < COMPLETION_NUM_CYCLES){
        completion_counter++;
    }
    else{
        total_completion_time = xTaskGetTickCount();
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

void task_resume(edd_task_t* sender){
    //printf("here 7777");
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

void task_wait_for_evt(edd_task_t* sender){
    time_t lateness = INT_MIN;
  //  printf("here 8888");
    if(sender->first_time){
        sender->first_time = false;
    }
    else{
        lateness = xTaskGetTickCount() - sender->deadline;
        #ifndef EDD_ENABLED
        lateness_file = fopen(lateness_file_name, "a");
        lateTask_file = fopen(lateTask_file_name, "a");
        #endif
        if(lateness > max_lateness){
           printf("new max lateness: %d\n", lateness);
          //  late_file = fopen(late_file_name, "a");
           //     fprintf(late_file, "new max lateness: %d\n", (int)lateness);
           //   fclose(late_file);
            max_lateness = lateness;
            fprintf(lateness_file, "%d\n", max_lateness);
        }
        if(lateness > 0){
            num_late_tasks++;
            fprintf(lateTask_file, "%d\n", num_late_tasks);
        }
        if(completion_counter < COMPLETION_NUM_CYCLES){
            completion_counter++;
        }
        else{
            total_completion_time = xTaskGetTickCount();
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

void task_got_evt(edd_task_t* sender){
    //printf("here 9999");
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
    //printf("here 9111");
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
    //printf("here 5222");
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
            //printf("here 3434");
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