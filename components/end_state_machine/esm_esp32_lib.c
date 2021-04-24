#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "esp_err.h"
#include "esm_esp32_lib.h"
#include "esp_log.h"

static const char *TAG 		= "ESM_LIB";

#define INVALID_SIGNAL  65535

//---------------- leave function undefined ----------------------------------
void  __attribute__((weak)) catch_error_leave(void * context) {
	ESP_LOGE(TAG, "\r\ncatch_error_leave");
	ESP_ERROR_CHECK (ESP_FAIL);	}

//---------------- jump function undefined ----------------------------------
void  __attribute__((weak)) catch_error_jump(void * context) {
	ESP_LOGE(TAG, "\r\ncatch_error_jump");
	ESP_ERROR_CHECK (ESP_FAIL); }

/* -----------------  exe function undefined ---------------------------------
context is a pointer to state  without exr function (for debug convinient) */
uint16_t  __attribute__((weak)) catch_error_exe(void * context) {
  ESP_LOGE(TAG, "\r\ncatch_error_exe. state");
  ESP_ERROR_CHECK (ESP_FAIL);
  return (INVALID_SIGNAL); }
/*-----------------  catch wrong add ---------------------------------
context is a pointer to wrong state or wrong signal which is duplicated (for debug convinient)*/
void  __attribute__((weak)) catch_wrong_add(void * context) {
  ESP_LOGE(TAG, "\r\ncatch_error_exe state or signal");
  ESP_ERROR_CHECK (ESP_FAIL); }
//-----------------------------------------------------------------------------------------------------
esm_t  create_esm(uint16_t total_states, uint16_t total_signals)
{  
  //esp_log_level_set("ESM_LIB", ESP_LOG_WARN);
  static const char not_dscr[] = "NOT DESCRIBE FUNC";
  uint32_t  jump_table_size   = total_states * total_signals * sizeof(signal_state_t);
  uint32_t  state_table_size  = total_states * sizeof(state_esm_t);
  esm_t instance = NULL;
  instance = malloc(sizeof(esm_inst_t) + jump_table_size + state_table_size - sizeof(signal_state_t) - sizeof(state_esm_t));
  if (instance)
  {
  	instance->xmut_esm =  xSemaphoreCreateMutex();
  	ESP_ERROR_CHECK(instance->xmut_esm == NULL);;
    instance->tot_state = total_states;
    instance->tot_signal = total_signals;
    instance->block_size = sizeof(state_esm_t) + total_signals * sizeof(signal_state_t);

    for (uint16_t i=0; i < total_states; i++)
    {
      state_esm_t   *p_stt = (state_esm_t *)((uint8_t*)&instance->state_first + i * instance->block_size);
      *p_stt = (state_esm_t){.state = i, .exe_func = catch_error_exe, .enter_func = catch_error_jump,  .name = not_dscr};
      
      signal_state_t  *p_sig = (signal_state_t *)((uint8_t*)instance->signal_first + i * instance->block_size);

      for (uint16_t j=0; j < total_signals; j++)
        p_sig[j] = (signal_state_t){.signal = j, .new_state = instance->tot_state, .leave_func = catch_error_leave};
    }
  } else
  	  {
	  	  ESP_LOGE(TAG, "\r\nnot enough memory");
	  	  ESP_ERROR_CHECK (ESP_FAIL);
  	  }
  return (instance);
}
//-----------------------------------------------------------------------------------------------------
static const char *get_name_from_state(esm_t esm_inst, uint16_t stnum)
{
  state_esm_t *req_state = (state_esm_t *)((uint8_t*)&esm_inst->state_first + stnum * esm_inst->block_size);
  return (req_state->name);	}
//-----------------------------------------------------------------------------------------------------
void destroy_esm(esm_t esm_inst)
{
	if (esm_inst)
	{
		vSemaphoreDelete(esm_inst->xmut_esm);
		free(esm_inst);
	}
}
//-----------------------------------------------------------------------------------------------------
uint16_t get_esm_state(esm_t esm_inst)
{
	xSemaphoreTake(esm_inst->xmut_esm, portMAX_DELAY);
	uint16_t stt = (esm_inst->curr_state);
	xSemaphoreGive(esm_inst->xmut_esm);
	return (stt);
}
//-----------------------------------------------------------------------------------------------------
void set_esm_state(esm_t esm_inst, uint16_t new_state)
{
	xSemaphoreTake(esm_inst->xmut_esm, portMAX_DELAY);
  if (new_state < esm_inst->tot_state)
      esm_inst->curr_state = new_state;
  else
  	  {
		  ESP_LOGE(TAG, "\r\ninvalid state");
		  ESP_ERROR_CHECK (ESP_FAIL);
	  }
  xSemaphoreGive(esm_inst->xmut_esm);
}


//-----------------------------------------------------------------------------------------------------
void add_state(esm_t esm_inst, state_esm_t state_dscr, uint16_t signals_qtt, ...)
{
  va_list argptr;
  va_start (argptr,signals_qtt);

  if (state_dscr.state >= esm_inst->tot_state)
  {
	  ESP_LOGE(TAG, "\r\ninvalid state");
	  ESP_ERROR_CHECK (ESP_FAIL);
  }
  
  state_esm_t   *p_stt = (state_esm_t *)((uint8_t*)&esm_inst->state_first + state_dscr.state * esm_inst->block_size);
  if (p_stt->exe_func != catch_error_exe)
    catch_wrong_add(&state_dscr.state);
  *p_stt = state_dscr;

  signal_state_t  *p_sig = (signal_state_t *)((uint8_t*)esm_inst->signal_first + state_dscr.state * esm_inst->block_size);
  for (; signals_qtt; signals_qtt--)
  {
    signal_state_t ssp = va_arg(argptr,signal_state_t);
    if ((ssp.signal >= esm_inst->tot_signal) || (ssp.new_state >=  esm_inst->tot_state))
    {
		  ESP_LOGE(TAG, "\r\nsignal jump describe wrong");
		  ESP_ERROR_CHECK (ESP_FAIL);
    }
   
    if(p_sig[ssp.signal].new_state != esm_inst->tot_state)
      catch_wrong_add(&p_sig[ssp.signal].new_state);
    p_sig[ssp.signal] = ssp;
  }
  va_end(argptr);
}


//-----------------------------------------------------------------------------------------------------
void execute_esm(esm_t esm_inst)
{
	xSemaphoreTake(esm_inst->xmut_esm, portMAX_DELAY);
  uint16_t sign;
  state_esm_t *active_state = (state_esm_t *)((uint8_t*)&esm_inst->state_first + esm_inst->curr_state * esm_inst->block_size);
  if (active_state->exe_func)
    sign = active_state->exe_func(esm_inst->context);
  else
  {
	  ESP_LOGE(TAG, "\r\nexecute function not defined");
	  ESP_ERROR_CHECK (ESP_FAIL);
  }

  signal_state_t  *active_signs = (signal_state_t *)((uint8_t*)&esm_inst->signal_first[sign] + esm_inst->curr_state * esm_inst->block_size);
  if (active_signs->new_state < esm_inst->tot_state)
  {
    if (active_signs->leave_func)
      active_signs->leave_func(esm_inst->context);

    ESP_LOGI(TAG, "%s: signal %d, state %s -> %s ", __func__, sign,
                      get_name_from_state(esm_inst, esm_inst->curr_state),
                      get_name_from_state(esm_inst, active_signs->new_state));
    esm_inst->curr_state = active_signs->new_state;
    active_state = (state_esm_t *)((uint8_t*)&esm_inst->state_first + esm_inst->curr_state * esm_inst->block_size);

    if (active_state->enter_func)  active_state->enter_func(esm_inst->context);
  } else
	  ESP_LOGD(TAG, "%s: signal %d, state %d not describe ", __func__, sign, active_signs->new_state);
  xSemaphoreGive(esm_inst->xmut_esm);
}
