

#include "app.h"
#include "edd.h"

//stuff for hardware task
static xQueueHandle hw_queue_handle;
static hw_evt_t events[NUM_EVENTS]; //this is populated before the scheduler starts, in order of timestamp
static ppg_sample_t ppg_samples[NUM_PPG_SAMPLES];
static imu_sample_t imu_samples[NUM_IMU_SAMPLES];
static int curr_ppg = 0;
static int curr_imu = 0;
static int curr_evt = 0;

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
    for (int i = 0; i < NUM_EVENTS; ++i) {
        if (i % 2 == 0) { //TODO: make this more realistic
            events[i].event_type = HW_EVT_LONG_PRESS;
        }
        else {
            events[i].event_type = HW_EVT_SHORT_PRESS;
        }
    }

    for (int i = 0; i < NUM_PPG_SAMPLES; ++i) {
        ppg_samples[i] = i; //TODO: make this more realistic
    }

    for (int i = 0; i < NUM_IMU_SAMPLES; ++i) {
        imu_samples[i] = i; //TODO: make this more realistic
    }

    hw_queue_handle = xQueueCreate(10, sizeof(task_msg_t));
    act_queue_handle = xQueueCreate(10, sizeof(task_msg_t));
    hrm_queue_handle = xQueueCreate(10, sizeof(task_msg_t));
    app_queue_handle = xQueueCreate(10, sizeof(task_msg_t));
    ui_queue_handle = xQueueCreate(10, sizeof(task_msg_t));

    act_task_data.deadline = 0;
    act_task_data.periodic = true;
    act_task_data.period = ACT_TASK_PERIOD;
    act_task_data.task_func = activity_task;
    act_task_data.task = &act_task_handle;

    hrm_task_data.deadline = 0;
    hrm_task_data.periodic = true;
    hrm_task_data.period - HRM_TASK_PERIOD;
    hrm_task_data.task_func = hr_monitor_task;
    hrm_task_data.task = &hrm_task_handle;

    app_task_data.deadline = 0;
    app_task_data.periodic = false;
    app_task_data.relative_deadline = APP_TASK_RELATIVE_DEADLINE;
    app_task_data.task_func = app_task;
    app_task_data.task = &app_task_handle;

    ui_task_data.deadline = 0;
    ui_task_data.periodic = false;
    ui_task_data.relative_deadline = UI_TASK_RELATIVE_DEADLINE;
    ui_task_data.task_func = ui_task;
    ui_task_data.task = &ui_task_handle;
}

void hw_timer_callback(TimerHandle_t xTimer) {
    //Send NEXT_EVENT message to hardware task
    task_msg_t msg = {};
    msg.type = HW_NEXT_EVENT;
    xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
}

// aperiodic
void hardware_task(void* pvParameters) { //this should have higher priority than rest, but lower priority than scheduler
    task_msg_t message = {};
    //send first hardware event
    hw_timer_callback(NULL);
    while (1) {
        //printf("hardware task\n");
        xQueueReceive(hw_queue_handle, &message, portMAX_DELAY); //wait for message

        switch (message.type) {
        case HW_NEXT_EVENT:
        {//adds local scoping to switch statement
            //printf("hardware task next event\n");
            hw_evt_t next_event = events[curr_evt];
            curr_evt++;
            if (curr_evt >= NUM_EVENTS) {
                curr_evt = 0; //loop around to beginning
            }
            TimerHandle_t timer = xTimerCreate("hw_evt_timer", next_event.time_stamp - xTaskGetTickCount(), pdFALSE, (void*)HW_EVT_TIMER, hw_timer_callback); //TODO: make sure this isn't negative, and that void* cast is OK
            xTimerStart(timer, 0);//TODO: need to delete this timer later

            switch (next_event.event_type) {
            case HW_EVT_SHORT_PRESS:
            {
                //send message to UI Task
                task_msg_t msg = {};
                msg.type = UI_SHORT_BUTTON_PRESS;
                xQueueSend(ui_queue_handle, &msg, portMAX_DELAY);
            }
            break;
            case HW_EVT_LONG_PRESS:
            {
                //send message to UI Task
                task_msg_t msg = {};
                msg.type = UI_LONG_BUTTON_PRESS;
                xQueueSend(ui_queue_handle, &msg, portMAX_DELAY);
            }
            break;
            }
        }
        break;
        case HW_IMU_REQUEST:
            //send message to Activity Task, data is vector of acceleration magnitudes
        {
            //printf("hardware task imu request\n");
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
            xQueueSend(act_queue_handle, &msg, portMAX_DELAY);
            //printf("message sent\n");
        }
        break;
        case HW_PPG_REQUEST:
            //send message to HR Monitor Task, data is vector of timestamps indicating the peaks in the waveform
        {
            //printf("hardware task ppg request\n");
            task_msg_t msg = {};
            msg.type = HRM_PPG_DATA;
            memcpy(msg.data, &ppg_samples[curr_ppg], sizeof(ppg_sample_t));
            curr_ppg++;
            if (curr_ppg >= NUM_PPG_SAMPLES) {
                curr_ppg = 0;
            }
            xQueueSend(hrm_queue_handle, &msg, portMAX_DELAY);
        }
        break;
        case HW_SCREEN_UPDATE:
            //printf("hardware task screen update\n");
            //print “screen” to console. “Screen” is just a string sent in the data field of this task
            printf("%s", (char*)message.data);
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
        if(new!=old){		
            if(new<sleeping){
                //printf("if new<sleeping\n");
                     msg.type = APP_ACT_TYPE_UPDATE;
                    memcpy(msg.data, &enum1, sizeof(int));
                    xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
            }
            else if(new<sedentary){
                     msg.type = APP_ACT_TYPE_UPDATE;
                    memcpy(msg.data, &enum2, sizeof(int));
                    xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
            }
            else{
                      msg.type = APP_ACT_TYPE_UPDATE;
                    memcpy(msg.data, &enum3, sizeof(int));
                    xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
            }
        }
        else {
            //printf("else\n");
            msg.type = APP_ACT_TYPE_UPDATE;
            memcpy(msg.data, &enum4, sizeof(int));
            //printf("copied %d\n", enum4);
            xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
            /*if(xQueueSend(app_queue_handle, &msg, portMAX_DELAY) != pdPASS){
                printf("not sent!\n");
                return 0;
            }*/
            //printf("message sent\n");
		}
		vTaskDelay(xDelay);
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
		message.type=HRM_PPG_DATA;

		xQueueSend(hw_queue_handle, &message, portMAX_DELAY);
		xQueueReceive(hrm_queue_handle, &message, portMAX_DELAY);
		for(int j = 0; j<10; j++){
			tensampleold[j] = tensamplenew[j];
		}
		
		for(int i = 0; i<9; i++){
		tensamplenew[i]=tensampleold[i+1];
		}

		memcpy(&tensamplenew[9],message.data,sizeof(imu_sample_t));

		for(int counter = 0; counter<9; counter++){
		hrdata += tensamplenew[counter+1]-tensamplenew[counter];
		}
		hrdata = hrdata/9;
		hrdata = 60000/hrdata;
		msg.type = APP_HEARTRATE;
		memcpy(msg.data, &hrdata, sizeof(hrdata));
		
		xQueueSend(app_queue_handle, &msg, portMAX_DELAY);	
		vTaskDelay(xDelay);
		
	}

}

void app_task(void* pvParameters) {
    task_msg_t in_message = {};
    task_msg_t out_message = {};

    char reminderStr[60];

    int goal_calories = 1000;
    int goal_steps = 100;
    int goal_movTime = 30*60;

    double hrate = 0;           // Heart rate in beats/min
    const float weight = 70;    // Weight in kilograms 
    const int age = 25;
    float act_time = 0;           // Time in hours
    int act_time_min = 0;
    int calories = 0;
    int act_type = -1;
    int curr_steps = 0;

    while(1){
        //printf("app task\n");
        xQueueReceive(app_queue_handle, &in_message, portMAX_DELAY);
        switch(in_message.type){
            case APP_HEARTRATE:
                hrate = (uintptr_t) in_message.data;
                
                out_message.type = APP_HEARTRATE;
                memcpy(out_message.data, &hrate, sizeof(int));
                xQueueSend(ui_queue_handle, &out_message, portMAX_DELAY);
                break;

            case APP_ACT_TYPE_UPDATE:
                act_type = (uintptr_t) in_message.data;

                if(act_type >= 3){
                    act_time_min++;
                    curr_steps += 10;
                }
                
                act_time = act_time_min / 60;

                // Suppose person wearing device is a man
                calories = ((-55.0969 + 0.6309*hrate + 0.1988*weight
                            + 0.2017*age)/4.184) * 60 * act_time;

                // Suppose person wearing device is a woman
                /*calories = ((-20.4022 + 0.4472*hrate - 0.1263*weight
                            + 0.074*age)/4.184) * 60 * act_time;*/
                break;
        }

        // Reminders
        out_message.type = APP_REMINDER;
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
        xQueueSend(ui_queue_handle, &out_message, portMAX_DELAY);

        if(act_time_min >= goal_movTime){
            strcpy(reminderStr, "Way to go! Goal movement time completed!\n");
            memcpy(out_message.data, reminderStr, strlen(reminderStr)+1);
        }
        else{
            char reminder[] = "Hey there! Let's stand up for a bit. Current movement time: ";
            char c[10];
            sprintf(c, "%i", act_time_min);
            strncat(reminder, &c, sizeof(int));
            strncat(reminder, "\n", sizeof(int));
            
            strcpy(reminderStr, &reminder);
            memcpy(out_message.data, reminderStr, strlen(reminderStr)+1);
        }
        xQueueSend(ui_queue_handle, &out_message, portMAX_DELAY);

        if(calories >= goal_calories){
            strcpy(reminderStr, "Hurray! You are the best! Goal calories completed!\n");
            memcpy(out_message.data, reminderStr, strlen(reminderStr)+1);
        }
        else{
            char reminder[] = "Let's try and do some quick exercises! Current calories: ";
            char c[10];
            sprintf(c, "%i", calories);
            strncat(reminder, &c, sizeof(int));
            strncat(reminder, "\n", sizeof(int));

            strcpy(reminderStr, &reminder);
            memcpy(out_message.data, reminderStr, strlen(reminderStr)+1);
        }
        xQueueSend(ui_queue_handle, &out_message, portMAX_DELAY);
    }
}

void ui_task(void* pvParameters) {
    task_msg_t message = {};
    task_msg_t msg = {};
    char messageStr[30]; 

    while (1) {
        //printf("ui task\n");
        xQueueReceive(ui_queue_handle, &message, portMAX_DELAY); //wait for message

        switch (message.type) {

            // hardware events 
        case UI_SHORT_BUTTON_PRESS:
            msg.type = HW_SCREEN_UPDATE;
            strcpy(messageStr, "Screen toggle on/off"); 
            memcpy(msg.data, messageStr, strlen(messageStr)+1);
            xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
            break;

        case UI_LONG_BUTTON_PRESS:
            msg.type = HW_SCREEN_UPDATE;
            strcpy(messageStr, "Device shut off"); 
            memcpy(msg.data, messageStr, strlen(messageStr)+1);
            xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
            break;

            // app events 
        case APP_REMINDER:
            msg.type = HW_SCREEN_UPDATE;
            strcpy(messageStr, "Reminder from App: "); 
            strcat(messageStr, message.data);
            memcpy(msg.data, messageStr, strlen(messageStr)+1);
            xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
            break;

        case APP_HEARTRATE:
            msg.type = HW_SCREEN_UPDATE;
            strcpy(messageStr, "Heart rate: "); 
            strcat(messageStr, message.data);
            memcpy(msg.data, messageStr, strlen(messageStr)+1);
            xQueueSend(hw_queue_handle, &msg, portMAX_DELAY);
            break;
        }
    }
}
