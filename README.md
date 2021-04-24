## Useful library 'End State machine'
End state machine example on ESP32 platrorm.  
I propouse a end state machine library, and how it use in example of button library and LEDs library.  
A state machine is described by set of states. There are execute a own function in each state. Each fuction have a set of signals. One of this signal should be returned as function terminated. And depending of returned signal the state machine going to other state. When this  jump is occurred a linked function is executing. For each jump (for example state A changed to state B) you can attach a special function. This function will be executed one time when A->B.  
How you see, my library is useful for describe a state maschine in convinient form like a table: States contained in rows, and signals in cols. And in cells where the state intersect with the signals a linked function are attached.      
The library use a clear C language and can be port to other platform.  

## API description  
Before use you should create machine:   
```create_esm(uint16_t total_states, uint16_t total_signals); ```  
this function allocate memory and return a handler.  

Next you create a several states:    
``` add_state(esm_t esm_inst, state_esm_t state_dscr, uint16_t signals_qtt, ...);```  
this function create one state and describe a several signals.  

Next you should set a begin state by:  
```set_esm_state(esm_t esm_inst, uint16_t new_state);```  

And last step: you must execute machine by:   
``` execute_esm(esm_t esm_inst);``` each RTOS tick for example, or only after interrupt. It depend on application logic.  

## How build
1. Install ESP-IDF SDK: "https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html"  
2. Use a DevKit or a custom board. I used a board like this: "https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/modules-and-boards.html#esp-wrover-kit-v4-1"  
3. Connect a button between GPIO25 and 3.3V  
4. Connect a LED between GPIO26 and 3.3V  
5. Compile and flash  
6. You can remap GPIO in defines  
