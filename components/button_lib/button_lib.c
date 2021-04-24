#include <stdint.h>
#include <stdbool.h>
#include "esm_esp32_lib.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "button_lib.h"

#define ESP_INTR_FLAG_DEFAULT	0
#define EVENT_BY_BUTTON			1
#define EVENT_BY_TIMER			2

typedef enum
{
	SIGNAL_NONE,
	SIGNAL_1,
	SIGNAL_TO,

	TOTAL_SIGNALS
} btn_signal_t;

typedef enum
{
	INITIAL_CHECK,
	WAIT_PRESS,
	WAIT_RELEASE_FIRST,
	WAIT_PRESS_SEC,
	WAIT_RELEASE_SEC,
	WAIT_LONG,
	WAIT_RELEASE_LONG,

	TOTAL_STATES
} btn_state_t;

struct btn_s
{
	esm_t						esm_inst;
	esp_timer_create_args_t		sequence_tmr_param;
	esp_timer_create_args_t		jitter_tmr_param;
	esp_timer_handle_t 			sequence_tmr_h;
	esp_timer_handle_t 			jitter_tmr_h;
	btn_init_param_t			btn_param;
	bool						timer_expiered;
	bool						jitter_active;
	bool						gpio_state;
};

static const char *TAG 		= "BUTTON_LIB";

ESP_EVENT_DEFINE_BASE(BTN_BASE_EVT);
//***************************************************************************************
uint16_t waitPress(void *context)  {
	btn_h	active_instance = (btn_h)context;
	if (active_instance->gpio_state)	//pressed ?
	{
		esp_timer_stop(active_instance->sequence_tmr_h);
		active_instance->timer_expiered = false;
		esp_timer_start_once(active_instance->sequence_tmr_h, active_instance->btn_param.dbl_time);
		button_info_t	inf= {.gpio_num = active_instance->btn_param.gpio_num,	.evt = BUTTON_DOWN };
		if( xQueueSend(active_instance->btn_param.button_queue, ( void * )&inf, 0 ) != pdPASS )
			ESP_LOGW(TAG,"button queue is full");
		return (SIGNAL_1);
	} return (SIGNAL_NONE);	}
//-------------------------------
uint16_t waitRelease(void *context) {
	btn_h	active_instance = (btn_h)context;
	if (active_instance->timer_expiered)
	{
		active_instance->timer_expiered = false;
		return (SIGNAL_TO);
	}
	if (!active_instance->gpio_state)
	{
		button_info_t	inf= {.gpio_num = active_instance->btn_param.gpio_num,	.evt = BUTTON_UP };
		if( xQueueSend(active_instance->btn_param.button_queue, ( void * )&inf, 0 ) != pdPASS )
			ESP_LOGW(TAG,"button queue is full");
		return (SIGNAL_1);	//single click recognize
	}
	return (SIGNAL_NONE);	}
//-------------------------------
uint16_t waitPressSecond(void *context)  {
	btn_h	active_instance = (btn_h)context;
	if (active_instance->timer_expiered)
	{
		button_info_t	inf= {.gpio_num = active_instance->btn_param.gpio_num,	.evt = BUTTON_DOWN };
		if( xQueueSend(active_instance->btn_param.button_queue, ( void * )&inf, 0 ) != pdPASS )
			ESP_LOGW(TAG,"button queue is full");
		return (SIGNAL_TO);
	}
	if (active_instance->gpio_state)		return (SIGNAL_1);	//second press click recognize
	return (SIGNAL_NONE);	}
//-------------------------------
void long_timer_start(void *context) {
	btn_h	active_instance = (btn_h)context;
	esp_timer_stop(active_instance->sequence_tmr_h);
	esp_timer_start_once(active_instance->sequence_tmr_h, active_instance->btn_param.long_time);
	active_instance->timer_expiered = false; }
//-------------------------------
void send_single(void *context) {
	btn_h	active_instance = (btn_h)context;
	button_info_t	inf= {.gpio_num = active_instance->btn_param.gpio_num,	.evt = SINGLE_CLICK };
	if( xQueueSend(active_instance->btn_param.button_queue, ( void * )&inf, 0 ) != pdPASS )
		ESP_LOGW(TAG,"button queue is full");
	}
//-------------------------------
void send_double(void *context) {
	btn_h	active_instance = (btn_h)context;
	button_info_t	inf= {.gpio_num = active_instance->btn_param.gpio_num,	.evt = DOUBLE_CLICK };
	if( xQueueSend(active_instance->btn_param.button_queue, ( void * )&inf, 0 ) != pdPASS )
		ESP_LOGW(TAG,"button queue is full");	}
//-------------------------------
void send_long(void *context) {
	btn_h	active_instance = (btn_h)context;
	button_info_t	inf= {.gpio_num = active_instance->btn_param.gpio_num,	.evt = LONG_CLICK };
	if( xQueueSend(active_instance->btn_param.button_queue, ( void * )&inf, 0 ) != pdPASS )
		ESP_LOGW(TAG,"button queue is full");}
//-------------------------------
void timer_stop(void *context) {
	btn_h	active_instance = (btn_h)context;
	esp_timer_stop(active_instance->sequence_tmr_h); }
//-------------------------------
static void sec_tmr_cb(void *arg){
	btn_h	active_instance = (btn_h)arg;
	active_instance->timer_expiered = true;
	ESP_ERROR_CHECK (esp_event_post(BTN_BASE_EVT, EVENT_BY_TIMER, (void*) &active_instance, sizeof(void*), 0));	}
//-------------------------------
static void jitter_tmr_cb(void *arg)	{
	// definite and send a stable state of button (after jitter)
	btn_h	active_instance = (btn_h)arg;
	active_instance->gpio_state = !(((bool)gpio_get_level(active_instance->btn_param.gpio_num)) ^ (active_instance->btn_param.active_level));
	active_instance->jitter_active = false;
	ESP_ERROR_CHECK (esp_event_post(BTN_BASE_EVT, EVENT_BY_BUTTON, (void*) &active_instance, sizeof(void*), 0));

}
//--------------------------------------------------------------
static void  gpio_isr_handler(void* arg)	{	// relaunch timer if gpio state is changed
	btn_h	active_instance = (btn_h)arg;
	esp_timer_stop(active_instance->jitter_tmr_h);
	active_instance->jitter_active = true;
	esp_timer_start_once(active_instance->jitter_tmr_h, active_instance->btn_param.jitter_time);
}
//--------------------------------------------------------------
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)	{
	// handling a sec_tmr_cb and jitter_tmr_cb event in event_loop cycle
	if (event_base == BTN_BASE_EVT)
	{
		btn_h	active_instance = (btn_h)*((btn_h*)event_data);
		execute_esm(active_instance->esm_inst);
	}
}
//*****************************************************************************************************
btn_h button_create(btn_init_param_t *param)
{
	btn_h	instance = NULL;
	bool err = false;
	instance = malloc(sizeof(struct btn_s));
	do
	{
		if (!instance)	break;
		memset(instance, 0, sizeof(struct btn_s));
		memcpy(&instance->btn_param, param, sizeof (btn_init_param_t));
		instance->esm_inst = create_esm(TOTAL_STATES, TOTAL_SIGNALS);
		if (!instance->esm_inst)
		{
			err = true;
			break;
		}
		// timer configuration
		instance->esm_inst->context = instance;
		esp_timer_init();
		instance->sequence_tmr_param.arg = instance;
		instance->sequence_tmr_param.callback = sec_tmr_cb;
		instance->sequence_tmr_param.dispatch_method = ESP_TIMER_TASK;
		instance->sequence_tmr_param.name = "btnEsmTmr";
		ESP_ERROR_CHECK (esp_timer_create((const esp_timer_create_args_t*)&instance->sequence_tmr_param, &instance->sequence_tmr_h));

		// jitter protect timer configure
		instance->jitter_tmr_param.arg = instance;
		instance->jitter_tmr_param.callback = jitter_tmr_cb;
		instance->jitter_tmr_param.dispatch_method = ESP_TIMER_TASK;
		instance->jitter_tmr_param.name = "jitterTmr";
		ESP_ERROR_CHECK(esp_timer_create((const esp_timer_create_args_t*)&instance->jitter_tmr_param, &instance->jitter_tmr_h));

		// register event for execute end-state machine by events
		ESP_ERROR_CHECK (esp_event_handler_register(BTN_BASE_EVT, EVENT_BY_BUTTON, event_handler, NULL));
		ESP_ERROR_CHECK (esp_event_handler_register(BTN_BASE_EVT, EVENT_BY_TIMER, event_handler, NULL));

		// button configuration
		ESP_ERROR_CHECK(gpio_set_direction(instance->btn_param.gpio_num, GPIO_MODE_INPUT));
		ESP_ERROR_CHECK(gpio_set_intr_type(instance->btn_param.gpio_num, GPIO_INTR_ANYEDGE));
		ESP_ERROR_CHECK(gpio_set_pull_mode(instance->btn_param.gpio_num, instance->btn_param.pull));
		esp_err_t res = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
		if ((res != ESP_OK) && (res != ESP_ERR_INVALID_STATE))
			ESP_ERROR_CHECK(res);
		ESP_ERROR_CHECK(gpio_isr_handler_add(instance->btn_param.gpio_num, gpio_isr_handler, (void*)instance));
		ESP_ERROR_CHECK(gpio_intr_enable(instance->btn_param.gpio_num));

		// end state machine filling
		add_state(instance->esm_inst, (state_esm_t) {.state = WAIT_PRESS, .exe_func = waitPress,  .enter_func = timer_stop, .name = "WAIT_PRESS"}, 1,
		                    (signal_state_t){.signal = SIGNAL_1, 	.new_state = WAIT_RELEASE_FIRST} );

		add_state(instance->esm_inst, (state_esm_t) {.state = WAIT_RELEASE_FIRST, .exe_func = waitRelease,  .name = "WAIT_RELEASE_FIRST"}, 2,
							(signal_state_t){.signal = SIGNAL_1, 	.new_state = WAIT_PRESS_SEC},
				           	(signal_state_t){.signal = SIGNAL_TO, 	.new_state = WAIT_LONG} );	//click long event

		add_state(instance->esm_inst, (state_esm_t) {.state = WAIT_PRESS_SEC, .exe_func = waitPressSecond,  .name = "WAIT_PRESS_SEC"}, 2,
						    (signal_state_t){.signal = SIGNAL_TO, 	.new_state = WAIT_PRESS, 			.leave_func = send_single},
							(signal_state_t){.signal = SIGNAL_1, 	.new_state = WAIT_RELEASE_SEC} );

		add_state(instance->esm_inst, (state_esm_t) {.state = WAIT_RELEASE_SEC, .exe_func = waitRelease,  .name = "WAIT_RELEASE_SEC"}, 2,
							(signal_state_t){.signal = SIGNAL_1, 	.new_state = WAIT_PRESS, 	.leave_func = send_double},
							(signal_state_t){.signal = SIGNAL_TO, 	.new_state = WAIT_LONG, 	.leave_func = send_single} );

		add_state(instance->esm_inst, (state_esm_t) {.state = WAIT_LONG, .exe_func = waitRelease, .enter_func = long_timer_start, .name = "WAIT_LONG_RELEASE"}, 2,
							(signal_state_t){.signal = SIGNAL_1, 	.new_state = WAIT_PRESS, 		.leave_func = send_single},
							(signal_state_t){.signal = SIGNAL_TO, 	.new_state = WAIT_RELEASE_LONG, .leave_func = send_long} );

		add_state(instance->esm_inst, (state_esm_t) {.state = WAIT_RELEASE_LONG, .exe_func = waitRelease,  .name = "WAIT_RELEASE_LONG"}, 1,
									(signal_state_t){.signal = SIGNAL_1, 	.new_state = WAIT_PRESS, 	.leave_func = send_double} );

		set_esm_state(instance->esm_inst, WAIT_PRESS);
		jitter_tmr_cb(instance);
	} while (0);

	if (err)
	{
		free (instance);
		instance = NULL;
	}
	return (instance);
}

uint8_t get_button_state(btn_h active_instance)
{
	if (active_instance->jitter_active) return (0xFF);
	return (uint8_t)(active_instance->gpio_state);
}
