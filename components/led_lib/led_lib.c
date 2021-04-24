#include <stdint.h>
#include <stdbool.h>
#include "esm_esp32_lib.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "led_lib.h"

//*************************************** GLOBAL VARIABLES ****************************
typedef enum
{
	SIGNAL_NONE,
	SIGNAL_TO,
	SIGNAL_END,

	TOTAL_SIGNALS
} led_signal_t;

typedef enum
{
	INACTIVE,
	LIGHTING,
	DARKING,

	TOTAL_STATES
} led_state_t;

struct led_s
{
	esm_t											esm_inst;
	esp_timer_create_args_t		sequence_tmr_param;
	esp_timer_handle_t 				sequence_tmr_h;
	led_init_param_t					led_param;
	uint32_t									rep_cnt;
	uint64_t									dark_us;
	uint64_t									light_us;
	uint8_t										priority;
	bool											timer_expiered;
};

static const char *TAG 		= "LED_LIB";
SemaphoreHandle_t xmut_led;
//-------------------------------------------------------------
static void sec_tmr_cb(void *arg) {
	led_h	active_instance = (led_h)arg;
	active_instance->timer_expiered = true;
	execute_esm(active_instance->esm_inst);
}
//-------------------------------
uint16_t inactive(void *context) {
	return (SIGNAL_NONE);	}
//-------------------------------------------------------------
static void turn_off(void *arg) {
	led_h	active_instance = (led_h)arg;
	esp_timer_stop(active_instance->sequence_tmr_h);
	gpio_set_level(active_instance->led_param.gpio_num, (bool)active_instance->led_param.active_level);
	active_instance->timer_expiered = false;
}
//-------------------------------------------------------------
static uint16_t delayLight(void *arg) {
	led_h	active_instance = (led_h)arg;
	if (active_instance->timer_expiered)
	{
		active_instance->timer_expiered = false;
		ESP_LOGD(TAG,"remaining %d flash", active_instance->rep_cnt);
		if (active_instance->rep_cnt)
		{
			--active_instance->rep_cnt;
			return (SIGNAL_TO);
		}
		else
			return (SIGNAL_END);
	}
	return (SIGNAL_NONE);
}
//-------------------------------------------------------------
static uint16_t delayDark(void *arg) {
	led_h	active_instance = (led_h)arg;
	if (active_instance->timer_expiered)
	{
		active_instance->timer_expiered = false;
		return (SIGNAL_TO);
	}
	return (SIGNAL_NONE);
}
//-------------------------------------------------------------
static void dark_ini(void *arg) {
	led_h	active_instance = (led_h)arg;
	esp_timer_stop(active_instance->sequence_tmr_h);
	active_instance->timer_expiered = false;
	gpio_set_level(active_instance->led_param.gpio_num, (bool)active_instance->led_param.active_level);
	esp_timer_start_once(active_instance->sequence_tmr_h, active_instance->dark_us);
}
//-------------------------------------------------------------
static void light_ini(void *arg) {
	led_h	active_instance = (led_h)arg;
	esp_timer_stop(active_instance->sequence_tmr_h);
	active_instance->timer_expiered = false;
	gpio_set_level(active_instance->led_param.gpio_num, !((bool)active_instance->led_param.active_level));
	esp_timer_start_once(active_instance->sequence_tmr_h, active_instance->light_us);
}
//-------------------------------------------------------------
led_h led_create(led_init_param_t *param)
{
	led_h	instance = NULL;
	xmut_led = xSemaphoreCreateMutex();
	ESP_ERROR_CHECK(xmut_led == NULL);

	instance = malloc(sizeof(struct led_s));
	ESP_ERROR_CHECK(instance == NULL);
	memset(instance, 0, sizeof(struct led_s));
	memcpy(&instance->led_param, param, sizeof (led_init_param_t));
	instance->esm_inst = create_esm(TOTAL_STATES, TOTAL_SIGNALS);
	ESP_ERROR_CHECK(instance->esm_inst == NULL);
	// timer configuration
	instance->esm_inst->context = instance;
	esp_timer_init();
	instance->sequence_tmr_param.arg = instance;
	instance->sequence_tmr_param.callback = sec_tmr_cb;
	instance->sequence_tmr_param.dispatch_method = ESP_TIMER_TASK;
	instance->sequence_tmr_param.name = "ledEsmTmr";
	ESP_ERROR_CHECK (esp_timer_create((const esp_timer_create_args_t*)&instance->sequence_tmr_param, &instance->sequence_tmr_h));

	// led configuration
	ESP_ERROR_CHECK(gpio_set_direction(instance->led_param.gpio_num, GPIO_MODE_OUTPUT));
	ESP_ERROR_CHECK(gpio_set_intr_type(instance->led_param.gpio_num, GPIO_INTR_DISABLE));


	// end state machine filling
	add_state(instance->esm_inst, (state_esm_t) {.state = INACTIVE, .exe_func = inactive,	.enter_func = turn_off,	.name = "INACTIVE"}, 1,
						(signal_state_t){.signal = SIGNAL_TO, 	.new_state = INACTIVE} );

	add_state(instance->esm_inst, (state_esm_t) {.state = LIGHTING, .exe_func = delayLight,	.enter_func = light_ini, .name = "LIGHTING"}, 2,
						(signal_state_t){.signal = SIGNAL_END, 	.new_state = INACTIVE},
						(signal_state_t){.signal = SIGNAL_TO, 	.new_state = DARKING} );	//click long event

	add_state(instance->esm_inst, (state_esm_t) {.state = DARKING, .exe_func = delayDark, 	.enter_func = dark_ini,  .name = "DARKING"}, 1,
						(signal_state_t){.signal = SIGNAL_TO, 	.new_state = LIGHTING} );

	set_esm_state(instance->esm_inst, INACTIVE);
	return (instance);
}

//----------------------------------------
void led_launch(led_h inst, uint8_t prio, uint32_t on_time, uint32_t off_time, uint32_t _repeat)
{
	xSemaphoreTake(xmut_led, portMAX_DELAY);
	if ((get_esm_state(inst->esm_inst) == INACTIVE) || (prio >= inst->priority))
	{
		inst->light_us = on_time * 1000;
		inst->dark_us = off_time * 1000;
		inst->rep_cnt = _repeat;
		inst->priority = prio;
		light_ini(inst);
		set_esm_state(inst->esm_inst, LIGHTING);
	}
	xSemaphoreGive(xmut_led);
}

//-------------------------------------
void led_stop(led_h inst, uint8_t prio)
{
	xSemaphoreTake(xmut_led, portMAX_DELAY);
	if ((get_esm_state(inst->esm_inst) == INACTIVE) || (prio >= inst->priority))
	{
		turn_off(inst);
		set_esm_state(inst->esm_inst, INACTIVE);
	}
	xSemaphoreGive(xmut_led);
}


