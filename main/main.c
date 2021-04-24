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
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"

#include "button_lib.h"
#include "indicator_led.h"
//******************************************  DEFINES  ********************************
#define BUTTON_POWER	25

//*************************************** GLOBAL VARIABLES ****************************
static const char *TAG 		= "MAIN";

//*********************************** FUNCTION DEFINITIONS ****************************


//------------------------------------------------------------------
void button_task(void *pvParameters)
{
	button_info_t btn_info;
	QueueHandle_t queue_btn_task = NULL;

	// configuration input line 'power button'
	queue_btn_task = xQueueCreate(32, sizeof(button_info_t));	//for receive a buttons events
	if (!queue_btn_task)	ESP_ERROR_CHECK(ESP_FAIL);
	btn_init_param_t pwr_btn_init;
	pwr_btn_init.gpio_num = BUTTON_POWER;
	pwr_btn_init.pull = GPIO_FLOATING;
	pwr_btn_init.active_level = 1;
	pwr_btn_init.jitter_time 	= 50000;
	pwr_btn_init.dbl_time 		= 250000;
	pwr_btn_init.long_time		= 2000000;
	pwr_btn_init.button_queue = queue_btn_task;
	btn_h pwr_btn_h = button_create(&pwr_btn_init);
	if (!pwr_btn_h) ESP_ERROR_CHECK(ESP_FAIL);

	while (1)
	{
		if (xQueueReceive(queue_btn_task, &btn_info, portMAX_DELAY))
		{

			switch (btn_info.evt)
			{
				case LONG_CLICK:
					led_indicate(LED_CMD_SELFPOWER);
					ESP_LOGI(TAG, "LONG CLICK");
					break;

				case BUTTON_UP:
					ESP_LOGI(TAG, "BUTTON UP");
					break;

				case BUTTON_DOWN:
					ESP_LOGI(TAG, "BUTTON DOWN");
					break;

				case SINGLE_CLICK:
				{
					led_indicate(LED_CMD_KBRD);
					ESP_LOGI(TAG, "SINGLE CLICK");
				} break;

				case DOUBLE_CLICK:
					ESP_LOGI(TAG, "DOUBLE_CLICK");
					break;

				default:
					break;
			}//switch (btn_info.state)
		}
	}//while (1)
}




//------------------------------------------------------------------
void app_main(void)
{
	ESP_LOGI(TAG,"________________Free heap %d b ___________________\r\n", esp_get_free_heap_size());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	indicator_led_init();
	xTaskCreate (&button_task, "button_task", 4096, NULL, 1, NULL);

}

