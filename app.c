

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
enum activityType{Sleeping, Sedentary, Exercise, NotChanged};//TODO: Maybe better to change to common.h
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
    act_task_data.task = act_task_handle;

    hrm_task_data.deadline = 0;
    hrm_task_data.periodic = true;
    hrm_task_data.period - HRM_TASK_PERIOD;
    hrm_task_data.task_func = hr_monitor_task;
    hrm_task_data.task = hrm_task_handle;

    app_task_data.deadline = 0;
    app_task_data.periodic = false;
    app_task_data.relative_deadline = APP_TASK_RELATIVE_DEADLINE;
    app_task_data.task_func = app_task;
    app_task_data.task = app_task_handle;

    ui_task_data.deadline = 0;
    ui_task_data.periodic = false;
    ui_task_data.relative_deadline = UI_TASK_RELATIVE_DEADLINE;
    ui_task_data.task_func = ui_task;
    ui_task_data.task = ui_task_handle;
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
        xQueueReceive(hw_queue_handle, &message, portMAX_DELAY); //wait for message

        switch (message.type) {
        case HW_NEXT_EVENT:
        {//adds local scoping to switch statement
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
            //send message to UI Task
            task_msg_t msg = {};
            msg.type = ACT_IMU_DATA;
            memcpy(msg.data, &imu_samples[curr_imu], sizeof(imu_sample_t));
            curr_imu++;
            if (curr_imu >= NUM_IMU_SAMPLES) {
                curr_imu = 0;
            }
            xQueueSend(act_queue_handle, &msg, portMAX_DELAY);
        }
        break;
        case HW_PPG_REQUEST:
            //send message to HR Monitor Task, data is vector of timestamps indicating the peaks in the waveform
        {
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
            //print “screen” to console. “Screen” is just a string sent in the data field of this task
            printf("%s", (char*)message.data);
            break;
        }
    }
}

void generator_task(void* pvParameters) {
    while (1) {
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

	while(1){
		message.type = HW_IMU_REQUEST;
		xQueueSend(hw_queue_handle, &message, portMAX_DELAY);//give me my msg hw
		xQueueReceive(act_queue_handle, &message, portMAX_DELAY); //wait for message						
		switch((int)message.data){
			case Sleeping:
				msg.type = ACT_NUM_MESSAGES;
				memcpy(msg.data, Sleeping, sizeof(int));
				xQueueSend(app_task_handle, &msg, portMAX_DELAY);
			case Sedentary:
				msg.type = ACT_NUM_MESSAGES;
				memcpy(msg.data, Sedentary, sizeof(int));
				xQueueSend(app_task_handle, &msg, portMAX_DELAY);
			case Exercise:
				msg.type = ACT_NUM_MESSAGES;
				memcpy(msg.data, Exercise, sizeof(int));
				xQueueSend(app_task_handle, &msg, portMAX_DELAY);
		    	case NotChanged:
				msg.type = ACT_NUM_MESSAGES;
				memcpy(msg.data, NotChanged, sizeof(int));
				xQueueSend(app_task_handle, &msg, portMAX_DELAY);
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
		
		message.type=HRM_PPG_DATA;

		xQueueSend(hw_queue_handle, &message, portMAX_DELAY);
		xQueueReceive(hrm_queue_handle, &message, portMAX_DELAY);
		for(int j = 0; j<10; j++){
			tensampleold[j] = tensamplenew[j];
		}
		
		for(int i = 0; i<9; i++){
		tensamplenew[i]=tensampleold[i+1];
		}

		tensamplenew[9]= (int) message.data;

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

}

void ui_task(void* pvParameters) {
    task_msg_t message = {};
    task_msg_t msg = {};
    char messageStr[30]; 

    while (1) {
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
