//extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>
#include <stdint.h>
//}
/******************************************
 * 				definitions
 *****************************************/
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

typedef enum {
	HW_EVT_SHORT_PRESS,
	HW_EVT_LONG_PRESS,
} hw_evt_type_t;

typedef struct hw_evt {
	hw_evt_type_t event_type;
	time_t time_stamp;
	char* event_data;
} hw_evt_t;

#define NUM_EVENTS 10

void hw_timer_callback(TimerHandle_t xTimer);
void hardware_task (void* pvParameters);

/******************************************
 * 				variables
 *****************************************/

//stuff for hardware task
hw_evt_t events[NUM_EVENTS]; //this is populated before the scheduler starts, in order of timestamp
xTaskHandle hw_task_handle;
xQueueHandle hw_queue_handle;
int curr_evt = 0;

/******************************************
 * 				functions
 *****************************************/

void hw_timer_callback(TimerHandle_t xTimer){
	//Send NEXT_EVENT message to hardware task
}

// aperiodic
void hardware_task (void* pvParameters) { //this should have higher priority than rest, but lower priority than scheduler
	task_msg_t message = {}; //TODO: make proper data type
    while(1){
		xQueueReceive(hw_queue_handle, &message, portMAX_DELAY); //wait for message
		
        switch(message.type){
			case HW_NEXT_EVENT:
                {
                    hw_evt_t next_event = events[curr_evt];
                    curr_evt++;
                    TimerHandle_t timer = xTimerCreate("hw_evt_timer", next_event.time_stamp - xTaskGetTickCount(), pdFALSE, (void*)HW_EVT_TIMER, hw_timer_callback); //TODO: make sure this isn't negative, and that void* cast is OK
                    xTimerStart(timer,0);//TODO: need to delete this timer later
                    switch(next_event.event_type){
                        case HW_EVT_SHORT_PRESS:
                            //send message to UI Task
                            break;
                        case HW_EVT_LONG_PRESS:
                            //send message to UI Task
                            break;
                    }
                }
            break;
            case HW_IMU_REQUEST:
                //send message to Activity Task, data is vector of acceleration magnitudes
            break;
            case HW_PPG_REQUEST:
                //send message to HR Monitor Task, data is vector of timestamps indicating the peaks in the waveform
            break;
            case HW_SCREEN_UPDATE:
                //print “screen” to console. “Screen” is just a string sent in the data field of this task
            break;
        }
    }
}

int main(){
    return 0;
}