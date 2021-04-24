#ifndef LED_LIB_H__
#define LED_LIB_H__

#include <string.h>
#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

typedef struct
{
	gpio_num_t 			gpio_num;
	bool				active_level;	//0 or 1
} led_init_param_t;

typedef struct led_s *led_h;


/* ********************************************************
Function for create a LED instance
param [in] param - initalization parameter type 'led_init_param_t'
return: instance like a pointer if SUCCESS
 	 	NULL - if instance didn't created
Application can control a LED by 'led_launch()' or 'led_stop()'
 * ****************************************************** */
led_h led_create(led_init_param_t *param);


/* ********************************************************
Function for launch a sequence for LED
param [in] inst - instance of LED returned 'led_create()'
param [in] prio - priority this led command
param	[in] on_time - time period in ms whrn LED is light
param	[in] off_time - time period in ms whrn LED is dark
param	[in] _repeat - how many times LED will flash
 * ****************************************************** */
void led_launch(led_h inst, uint8_t prio, uint32_t on_time, uint32_t off_time, uint32_t _repeat);


/* ********************************************************
Function for terminated a LED sequence and turn LED off
param [in] inst - instance of LED returned 'led_create()'
param [in] prio - priority this led command
 * ****************************************************** */
void led_stop(led_h inst, uint8_t prio);

#endif	// LED_LIB_H__
