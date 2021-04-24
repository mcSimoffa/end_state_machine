#ifndef ESM_ESP32_LIB_H__
#define ESM_ESP32_LIB_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

typedef uint16_t (* exe_func_t)(void * context);   //type of execute function
typedef void (* jump_or_leave_t)(void * context);  //type of jump / leave function

typedef struct
{
  uint16_t            signal;
  uint16_t            new_state;
  jump_or_leave_t     leave_func; 
} signal_state_t;

typedef struct
{
  uint16_t            state;
  uint16_t            reserve;
  jump_or_leave_t     enter_func;
  exe_func_t          exe_func; 
  const char       	  *name;
} state_esm_t;

typedef struct
{
  uint16_t        	tot_state;
  uint16_t        	tot_signal;
  uint32_t        	block_size;
  uint16_t        	curr_state;
  uint16_t        	reserve;
  void			  			*context;
  SemaphoreHandle_t xmut_esm;
  state_esm_t     	state_first;
  signal_state_t  	signal_first[1];
} esm_inst_t;


typedef esm_inst_t *esm_t;

/********************************************************************
create end state machine
parameter [in] total_states - quantity of states ESM
          [in] total_signals - quantity of signals (results) used in ESM
return value - end state machine instance
********************************************************************/
esm_t  create_esm(uint16_t total_states, uint16_t total_signals);


/********************************************************************
Function for ESM destroy, and release memory to system
parameter [in] esm_inst end state machine instance
********************************************************************/
void destroy_esm(esm_t esm_inst);


/********************************************************************
You need execute this function by every tick
It's a driver of end state machine
parameter [in] esm_inst end state machine instance
********************************************************************/
void execute_esm(esm_t esm_inst);


/********************************************************************
Function to add a one state with description to base of ESM
parameter [in] esm_inst - end state machine instance
          [in] state_dscr - description of added state 
                            in a special form (in state_esm_t struct view)
          [in] signals_qtt - quantity of relevant signals (results) for adding state
          ... description of each signal in stuct 'signal_state_t' view
********************************************************************/
void add_state(esm_t esm_inst, state_esm_t state_dscr, uint16_t signals_qtt, ...);

/********************************************************************
Get the current state of end state machine
parameter [in] esm_inst end state machine instance
********************************************************************/
uint16_t get_esm_state(esm_t esm_inst);


/********************************************************************
Set  esm state a new value
parameter [in] - esm_inst end state machine instance
          [in] - new_state  new state
********************************************************************/
void set_esm_state(esm_t esm_inst, uint16_t new_state);

#endif	// ESM_ESP32_LIB_H__
