#ifndef BUTTON_LIB_H__
#define BUTTON_LIB_H__

#include <string.h>
#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(BTN_BASE_EVT);

typedef struct
{
	gpio_num_t 			gpio_num;
	bool				active_level;	//0 or 1
	gpio_pull_mode_t 	pull;
	uint32_t			jitter_time;
	uint32_t			dbl_time;
	uint32_t			long_time;
	QueueHandle_t 		button_queue;
} btn_init_param_t;

typedef enum
{
	BUTTON_DOWN,
	BUTTON_UP,
	SINGLE_CLICK,
	DOUBLE_CLICK,
	LONG_CLICK
} button_state_t;

typedef struct
{
	gpio_num_t 		gpio_num;
	button_state_t	evt;
} button_info_t;

typedef struct btn_s *btn_h;

/* ********************************************************
Function for create a button instance
param [in] param - initalization parameter type 'btn_init_param_t'
return: instance like a pointer if SUCCESS
 	 	NULL - if instance didn't created
Button events will be pushed to queue, specified in a 'param' structure.
Application should watch this queue
 * ****************************************************** */
btn_h button_create(btn_init_param_t *param);

/* ********************************************************
Function for read current state of button
param [in] active_instance - instance what returned after button_create() function
return: 255 - if button now have a not stable state like jitter
 	 	0 - if button isn't pressed
 	 	1 - if button is pressed
 * ****************************************************** */
uint8_t get_button_state(btn_h active_instance);

#endif	// BUTTON_LIB_H__
