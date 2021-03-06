 
#include "app.h"
#include "edd.h"
#include <math.h>
#define MEASURE_TASK_SWITCH
#ifdef MEASURE_TASK_SWITCH
char log_file_name[] = "tasks.log";
FILE* log_file;
#endif

 
//stuff for hardware task
static xQueueHandle hw_queue_handle;
static hw_evt_t events[NUM_EVENTS]; //this is populated before the scheduler starts, in order of timestamp
static ppg_sample_t ppg_samples[NUM_PPG_SAMPLES];
static imu_sample_t imu_samples[NUM_IMU_SAMPLES];
static int curr_ppg = 0;
static int curr_imu = 0;
static int curr_evt = 0;
static TimerHandle_t hw_timer;
 
//stuff for activity task
static xTaskHandle act_task_handle;
static xQueueHandle act_queue_handle;
typedef enum activityType{Sleeping, Sedentary, Exercise, NotChanged}activityType_t;//TODO: Maybe better to change to common.h
//stuff for hr monitor task
static xTaskHandle hrm_task_handle;
static xQueueHandle hrm_queue_handle;
 
//stuff for app task
static xTaskHandle app_task_handle;
static xQueueHandle app_queue_handle;
 
//stuff for ui task
static xTaskHandle ui_task_handle;
static xQueueHandle ui_queue_handle;
 
void init_app() {
    #ifdef MEASURE_TASK_SWITCH
    log_file = fopen(log_file_name, "w");
    #endif

    for (int i = 0; i < NUM_EVENTS; ++i) {
        if (i % 2 == 0) { //TODO: make this more realistic
            events[i].event_type = HW_EVT_LONG_PRESS;
        }
        else {
            events[i].event_type = HW_EVT_SHORT_PRESS;
        }
        events[i].time_stamp = i*870; //choose weird number
    }
 
    for (int i = 0; i < NUM_PPG_SAMPLES; ++i) {
 
        if(i<20){
        ppg_samples[i] = 70+(20-i); //TODO: make this more realistic
        }
        else if(i<40){
        ppg_samples[i] = 70+(45-i); //TODO: make this more realistic
        }
        else if(i<60){
        ppg_samples[i] = 70+(60-i); //TODO: make this more realistic
        }
        else if(i<80){
        ppg_samples[i] = 70+(75-i); //TODO: make this more realistic
        }
        else if(i<100){
        ppg_samples[i] = 70+(90-i); //TODO: make this more realistic
        }
 
 
    }
 
    for (int i = 0; i < NUM_IMU_SAMPLES; ++i) {
        imu_samples[i] = i; //TODO: make this more realistic
    }
 
    hw_queue_handle = xQueueCreate(MSG_QUEUE_SIZE, sizeof(task_msg_t));
    act_queue_handle = xQueueCreate(MSG_QUEUE_SIZE, sizeof(task_msg_t));
    hrm_queue_handle = xQueueCreate(MSG_QUEUE_SIZE, sizeof(task_msg_t));
    app_queue_handle = xQueueCreate(MSG_QUEUE_SIZE, sizeof(task_msg_t));
    ui_queue_handle = xQueueCreate(MSG_QUEUE_SIZE, sizeof(task_msg_t));
 
    act_task_data.deadline = ACT_TASK_PERIOD;
    act_task_data.periodic = true;
    act_task_data.period = ACT_TASK_PERIOD;
    act_task_data.relative_deadline = ACT_TASK_RELATIVE_DEADLINE;
    act_task_data.task_func = activity_task;
    act_task_data.task = &act_task_handle;
    act_task_data.first_time = true;
    act_task_data.max_Lateness =-1000;
    strcpy(act_task_data.name,"act");
    

    hrm_task_data.deadline = HRM_TASK_PERIOD;
    hrm_task_data.periodic = true;
    hrm_task_data.period = HRM_TASK_PERIOD;
    hrm_task_data.relative_deadline = HRM_TASK_RELATIVE_DEADLINE;
    hrm_task_data.task_func = hr_monitor_task;
    hrm_task_data.task = &hrm_task_handle;
    hrm_task_data.first_time = true;
    hrm_task_data.max_Lateness =-1000;
    strcpy(hrm_task_data.name,"hrm");


    app_task_data.deadline = 0;
    app_task_data.periodic = false;
    app_task_data.relative_deadline = APP_TASK_RELATIVE_DEADLINE;
    app_task_data.task_func = app_task;
    app_task_data.task = &app_task_handle;
    app_task_data.first_time = true;
    app_task_data.max_Lateness =-1000;
    strcpy(app_task_data.name,"app");


    ui_task_data.deadline = 0;
    ui_task_data.periodic = false;
    ui_task_data.relative_deadline = UI_TASK_RELATIVE_DEADLINE;
    ui_task_data.task_func = ui_task;
    ui_task_data.task = &ui_task_handle;
    ui_task_data.first_time = true;
    ui_task_data.max_Lateness =-1000;
    strcpy(ui_task_data.name,"ui");

}
 
void hw_timer_callback(TimerHandle_t xTimer) {
    //Send NEXT_EVENT message to hardware task
    task_msg_t msg = {};
    msg.type = HW_NEXT_EVENT;
 
 
    xTimerStop(hw_timer, portMAX_DELAY);
    xTimerDelete(hw_timer, portMAX_DELAY);
 
    xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
}
 
// aperiodic
void hardware_task(void* pvParameters) { //this should have higher priority than rest, but lower priority than scheduler
    task_msg_t message = {};
    //send first hardware event
    int i = 0;
    char is[100];
    task_msg_t temp_msg = {};
    temp_msg.type = HW_NEXT_EVENT;
    xQueueSend(hw_queue_handle, &temp_msg, portMAX_DELAY);
    time_t looptime= 0;
    while (1) {
        //printf("hardware task\n");
        xQueueReceive(hw_queue_handle, &message, portMAX_DELAY); //wait for message
        
        //printf("here 1");
        switch (message.type) {
        case HW_NEXT_EVENT:
        {//adds local scoping to switch statement
            //printf("hardware task next event\n");
            hw_evt_t next_event = events[curr_evt];
            curr_evt++;
            if (curr_evt >= NUM_EVENTS) {
                looptime = xTaskGetTickCount();
                curr_evt = 0; //loop around to beginning
            }
            int wait_time = next_event.time_stamp - xTaskGetTickCount()+looptime;
            if(wait_time < 0){
                wait_time = 0;
            }
            hw_timer = xTimerCreate("hw_evt_timer", wait_time, pdFALSE, (void*)HW_EVT_TIMER, hw_timer_callback); //TODO: make sure this isn't negative, and that void* cast is OK
            xTimerStart(hw_timer, portMAX_DELAY);//TODO: need to delete this timer later
            //printf("here 2");
            switch (next_event.event_type) {
            case HW_EVT_SHORT_PRESS:
            {
                //send message to UI Task
               // printf("SHORT");
                task_msg_t msg = {};
                msg.type = UI_SHORT_BUTTON_PRESS;
                //printf("here 3");
                xQueueSend(ui_queue_handle, &msg, portMAX_DELAY);
            }
            break;
            case HW_EVT_LONG_PRESS:
            {
                //send message to UI Task
               // printf("LONG");
                task_msg_t msg = {};
                msg.type = UI_LONG_BUTTON_PRESS;
                //printf("here 4");
                xQueueSend(ui_queue_handle, &msg, portMAX_DELAY);
            }
            break;
            }
        }
        break;
        case HW_IMU_REQUEST:
            //send message to Activity Task, data is vector of acceleration magnitudes
        {
           // printf("hardware task imu request\n");
            //send message to UI Task
            task_msg_t msg = {};
            msg.type = ACT_IMU_DATA;
            //imu_samples[curr_imu] = 40;
            memcpy(msg.data, &imu_samples[curr_imu], sizeof(imu_sample_t));
            //printf("%d\n", imu_samples[curr_imu]);
            curr_imu++;
            if (curr_imu >= NUM_IMU_SAMPLES) {
                curr_imu = 0;
            }
            //printf("%d\n", imu_samples[curr_imu]);
            //printf("here 5");
            xQueueSend(act_queue_handle, &msg, portMAX_DELAY);
            //printf("message sent\n");
        }
        break;
        case HW_PPG_REQUEST:
            //send message to HR Monitor Task, data is vector of timestamps indicating the peaks in the waveform
        {
           // printf("hardware task ppg request\n");
            task_msg_t msg = {};
            msg.type = HRM_PPG_DATA;
            memcpy(msg.data, &ppg_samples[curr_ppg], sizeof(ppg_sample_t));
            curr_ppg++;
            if (curr_ppg >= NUM_PPG_SAMPLES) {
                curr_ppg = 0;
            }
            //printf("here 6");
            xQueueSend(hrm_queue_handle, &msg, portMAX_DELAY);
        }
        break;
        case HW_SCREEN_UPDATE:
        {
           // printf("hardware task screen update\n");
            //print ???screen??? to console. ???Screen??? is just a string sent in the data field of this task
          //printf("here 7");
          memcpy(&is, message.data, sizeof(char[100]));
          //fprintf(log_file, "String: %s\n", is);
          printf("%s", is);
          //printf("here 6990");
        }
          break;
        }
    }
}
 
void generator_task(void* pvParameters) {
    while (1) {
        //printf("generator task\n");
        // Add each of the tasks to list using task_create 
        task_create(&act_task_data, "activity", BASE_APP_PRIORITY);
        task_create(&ui_task_data, "ui", BASE_APP_PRIORITY);
        task_create(&app_task_data, "app", BASE_APP_PRIORITY);
        task_create(&hrm_task_data, "hrm", BASE_APP_PRIORITY);
        vTaskDelete(generator_task_handle); //delete self
    }
}
 
void activity_task(void* pvParameters) {
	task_msg_t message = {};
	task_msg_t msg = {};
	const TickType_t xDelay = 500 / portTICK_PERIOD_MS;
    int sleeping = 0;
    int sedentary = 10;
    int exercise = 20;
    int new = 0;
    int old = 0;
    int tensampleold[10]={0,0,0,0,0,0,0,0,0,0};
	int tensamplenew[10]={0,0,0,0,0,0,0,0,0,0};
    int enum1 = 1;
    int enum2 = 2;
    int enum3 = 3;
    int enum4 = 4;
	static ppg_sample_t ppg_samples[NUM_PPG_SAMPLES];
	double hrdata=0;
 
	while(1){
        //printf("activity task\n");
        new=0;
        old=0;
		message.type = HW_IMU_REQUEST;
		xQueueSend(hw_queue_handle, &message, portMAX_DELAY);//give me my msg hw
		xQueueReceive(act_queue_handle, &message, portMAX_DELAY); //wait for message
        //printf("here 8");
        #ifdef MEASURE_TASK_SWITCH
        fprintf(log_file, "activity: %ld\n", xTaskGetTickCount());
        #endif	
        //printf("message received\n");
 
        for(int j = 0; j<10; j++){
			tensampleold[j] = tensamplenew[j];
            old+=tensamplenew[j];
            //printf("sample old: %d - sample new: %d - old: %d\n");
		}
        //printf("first for\n");
 
		for(int i = 0; i<9; i++){
		    tensamplenew[i]=tensampleold[i+1];
            new+=tensamplenew[i];
		}
        //printf("second for\n");
        int test = 0;
        //printf("test %d\n", test);
        memcpy(&test, message.data, sizeof(imu_sample_t));
        //printf("test %d\n", test);
		//tensamplenew[9]= (uintptr_t) message.data;
        //printf("%s\n", message.data);
        memcpy(&tensamplenew[9], message.data, sizeof(imu_sample_t));
        //printf("%d\n", tensamplenew[9]);
        new = new + tensamplenew[9];
        //printf("%d\n", new);
        old = old /10;
        new = new /10;
        //printf("before first if\n");
        //printf("here 9");
        if(new!=old){		
            if(new<sleeping){
                //printf("if new<sleeping\n");
                     msg.type = APP_ACT_TYPE_UPDATE;
 
                    memcpy(msg.data, &enum1, sizeof(activityType_t));
                     //printf("\nType 2 %d \n", enum1);
                    xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
            }
            else if(new<sedentary){
                     msg.type = APP_ACT_TYPE_UPDATE;
 
                    memcpy(msg.data, &enum2, sizeof(activityType_t));
                    //printf("\nType 2 %d \n", enum2);
                    xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
            }
            else{
                      msg.type = APP_ACT_TYPE_UPDATE;
 
                    memcpy(msg.data, &enum3, sizeof(activityType_t));
                    //printf("\nType 2 %d \n", enum3);
                    xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
            }
        }
        else {
            //printf("else\n");
            msg.type = APP_ACT_TYPE_UPDATE;
            memcpy(msg.data, &enum4, sizeof(int));
            //printf("copied %d\n", enum4);
            //printf("here 10");
            xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
            /*if(xQueueSend(app_queue_handle, &msg, portMAX_DELAY) != pdPASS){
                printf("not sent!\n");
                return 0;
            }*/
            //printf("message sent\n");
		}
		task_delay(&act_task_data);
	}
 
 
 
}
 
void hr_monitor_task(void* pvParameters){
	task_msg_t message = {};
	task_msg_t msg = {};
	 int tensampleold[10];
	int tensamplenew[10];
	static ppg_sample_t ppg_samples[NUM_PPG_SAMPLES];
	double hrdata=0;
	//What do I even make this
	//ask rory what he wanted for the task_delay 
	const TickType_t xDelay = 500 / portTICK_PERIOD_MS;
 
	while(1){
		//printf("hear rate task\n");
		msg.type=HW_PPG_REQUEST;
 //printf("here 12");
		xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
		xQueueReceive(hrm_queue_handle, &message, portMAX_DELAY);
        //printf("here 11");
        #ifdef MEASURE_TASK_SWITCH
        fprintf(log_file, "hrm: %ld\n", xTaskGetTickCount());
        #endif
		for(int j = 0; j<10; j++){
			tensampleold[j] = tensamplenew[j];
		}
 
		for(int i = 0; i<9; i++){
		tensamplenew[i]=tensampleold[i+1];
		}
 
		memcpy(&tensamplenew[9],message.data,sizeof(imu_sample_t));
 
		for(int counter = 0; counter<9; counter++){
		hrdata += tensamplenew[counter+1];
        //-tensamplenew[counter];
		}
		hrdata = hrdata/9;
        //hrdata = floor(hrdata*100)/100;
		//hrdata = 60000/hrdata;
 
		msg.type = APP_HEARTRATE;
		memcpy(msg.data, &hrdata, sizeof(hrdata));
 //printf("here 13");
		xQueueSend(app_queue_handle, &msg, portMAX_DELAY);	
		task_delay(&hrm_task_data);
 
	}
 
}
 
void app_task(void* pvParameters) {
    task_msg_t in_message = {};
    task_msg_t out_message = {};
 
 task_msg_t out_message1 = {};
 task_msg_t out_message2 = {};
    char reminderStr[100];
    char reminderstr1[100];
    char reminderstr2[100];
 
    int goal_calories = 1000;
    int goal_steps = 100;
    int goal_movTime = 2*60;
 
    double hrate = 0;           // Heart rate in beats/min
    const float weight = 70;    // Weight in kilograms 
    const int age = 25;
    float act_time = 0;           // Time in hours
    float act_time_min = 0;
    double calories = 0;
    int act_type = -1;
    int curr_steps = 0;
 
    while(1){
        //printf("app task\n");
        task_wait_for_evt(&app_task_data);
        xQueueReceive(app_queue_handle, &in_message, portMAX_DELAY);
        #ifdef MEASURE_TASK_SWITCH
        fprintf(log_file, "app: %d\n", xTaskGetTickCount());
        #endif
        task_got_evt(&app_task_data);
        //printf("here 14");
        switch(in_message.type){
            case APP_HEARTRATE:
                memcpy(&hrate, in_message.data,sizeof(double));
                //hrate = (uintptr_t) in_message.data;
                char c[10];
                sprintf(c, "%.0f \n", hrate);
                out_message.type = APP_HEARTRATE;
 
                memcpy(out_message.data, c, strlen(c)+1);
                //printf("here 15");
                xQueueSend(ui_queue_handle, &out_message, portMAX_DELAY);
                break;
 
            case APP_ACT_TYPE_UPDATE:
                memcpy(&act_type, in_message.data,sizeof(activityType_t));
                //act_type = (activityType_t)in_message.data;
 
                if(act_type == 3){
                    act_time_min++;
                    curr_steps += 10;
                }
 
                act_time = act_time_min / 60;
                if(hrate > 0 && hrate<100000){
                // Suppose person wearing device is a man
                    if (calories < goal_calories){
                    calories = ((-55.0969 + 0.6309*hrate + 0.1988*weight
                            + 0.2017*age)/4.184) * 60 * act_time;
                    }
                }
                // Suppose person wearing device is a woman
                /*calories = ((-20.4022 + 0.4472*hrate - 0.1263*weight
                            + 0.074*age)/4.184) * 60 * act_time;*/
                            //printf("here 16");
                break;
        }
 
        // Reminders
        out_message.type = APP_REMINDER;
         out_message1.type = APP_REMINDER;
          out_message2.type = APP_REMINDER;
        if(curr_steps >= goal_steps){
            strcpy(reminderStr, "You did it! Goal steps completed!\n");
            memcpy(out_message.data, reminderStr, strlen(reminderStr)+1);
        }
        else{
            char reminder[] = "Let's complete those steps! Current steps: ";
            char c[10];
            sprintf(c, "%i", curr_steps);
            strncat(reminder, &c, sizeof(int));
            strncat(reminder, "\n", sizeof(int));
 
            strcpy(reminderStr, &reminder);
            memcpy(out_message.data, reminderStr, strlen(reminderStr)+1);
        }
        //printf("here 17");
        xQueueSend(ui_queue_handle, &out_message, portMAX_DELAY);
 
        if(act_time_min >= goal_movTime){
            strcpy(reminderstr1, "Way to go! Goal movement time completed!\n");
            memcpy(out_message1.data, reminderstr1, strlen(reminderstr1)+1);
        }
        else{
            char reminder1[] = "Hey there! Let's stand up for a bit. Current movement time: ";
            char c[10];
            sprintf(c, "%d", (int) act_time_min);
            strncat(reminder1, &c, sizeof(int));
            strncat(reminder1, "\n", sizeof(int));
 
            strcpy(reminderstr1, &reminder1);
            memcpy(out_message1.data, reminderstr1, strlen(reminderstr1)+1);
        }
        //printf("here 18");
        xQueueSend(ui_queue_handle, &out_message1, portMAX_DELAY);
 
        if(calories >= goal_calories){
            strcpy(reminderstr2, "Hurray! You are the best! Goal calories completed!\n");
            memcpy(out_message2.data, reminderstr2, strlen(reminderstr2)+1);
        }
        else{
            char reminder2[] = "Let's try and do some quick exercises! Current calories: ";
            char c[10];
            sprintf(c, "%.2f", calories);
            strncat(reminder2, &c, sizeof(double));
            strncat(reminder2, "\n", sizeof(int));
 
            strcpy(reminderstr2, &reminder2);
            memcpy(out_message2.data, reminderstr2, strlen(reminderstr2)+1);
        }
        //printf("here 19");
        xQueueSend(ui_queue_handle, &out_message2, portMAX_DELAY);
    }
}
 
void ui_task(void* pvParameters) {
    task_msg_t message = {};
    task_msg_t msg = {};
    char messageStr[100]; 
 
    while (1) {
        //printf("ui task\n");
        task_wait_for_evt(&ui_task_data);
        xQueueReceive(ui_queue_handle, &message, portMAX_DELAY); //wait for message
        #ifdef MEASURE_TASK_SWITCH
        fprintf(log_file, "ui: %ld\n", xTaskGetTickCount());
        #endif
        task_got_evt(&ui_task_data);
 //printf("here 24");
        switch (message.type) {
 
            // hardware events 
        case UI_SHORT_BUTTON_PRESS:
            msg.type = HW_SCREEN_UPDATE;
            strcpy(messageStr, "Screen toggle on/off\n"); 
            memcpy(msg.data, messageStr, strlen(messageStr)+1);
           // printf("here 20");
            xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
            break;
 
        case UI_LONG_BUTTON_PRESS:
            msg.type = HW_SCREEN_UPDATE;
            strcpy(messageStr, "Device shut off\n"); 
            memcpy(msg.data, messageStr, strlen(messageStr)+1);
           // printf("here 21");
            xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
            break;
 
            // app events 
        case APP_REMINDER:
            msg.type = HW_SCREEN_UPDATE;
            //printf("here 224243");
            strcpy(messageStr, "Reminder from App: "); 
            strcat(messageStr, message.data);
            memcpy(msg.data, messageStr, strlen(messageStr)+1);
            //printf("here 22");
            xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
            break;
 
        case APP_HEARTRATE:
            msg.type = HW_SCREEN_UPDATE;
            //printf("here 2223");
            strcpy(messageStr, "Heart rate: "); 
            strcat(messageStr, message.data);
            memcpy(msg.data, messageStr, strlen(messageStr)+1);
            //printf("here 23");
            xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
            break;
        }
    }
}
