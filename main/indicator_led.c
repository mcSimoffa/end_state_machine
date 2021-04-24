#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"

#include "led_lib.h"
#include "indicator_led.h"

#define LED_PIN		26
//*************************************** GLOBAL VARIABLES ****************************
static const char *TAG 		= "IND_LED";
led_init_param_t  ledinit = {.gpio_num = LED_PIN, .active_level = 1};
led_h led = NULL;

//----------------------------------------------
void indicator_led_init()
{
	led = led_create(&ledinit);
	ESP_LOGI(TAG, "%s: LED init successfull", __func__);	//temporary take a power
}

//----------------------------------------------
void led_indicate(uint8_t type)
{
	switch (type)
	{
		case LED_CMD_ERR:
			led_launch(led, LED_CMD_ERR, 40, 40, UINT32_MAX);
			break;

		case LED_CMD_WARN:
			led_launch(led, LED_CMD_WARN, 150, 150, UINT32_MAX);
			break;

		case LED_CMD_POWER_ON:
			led_launch(led, LED_CMD_POWER_ON, 40, 200, UINT32_MAX);
			break;

		case LED_CMD_SELFPOWER:
			led_launch(led, LED_CMD_POWER_ON, 500, 500, UINT32_MAX);	// light LED as inform about 'self power OK'
			break;

		case LED_CMD_KBRD:
			led_launch(led, LED_CMD_KBRD, 100, 0, 1);
			break;

		default:
			break;
	}
}

//----------------------------------------------
void led_indicate_on_always()
{
	led_launch(led, LED_CMD_POWER_OFF, UINT32_MAX, 0, UINT32_MAX);
}

//----------------------------------------------
void led_off(uint8_t type)
{
	led_stop(led, type);	// dark LED after WIFI success
}

