#ifndef INDICATOR_LED_H_
#define INDICATOR_LED_H_

// LED command and priority (the big priority command can to cancel a low priority command)
#define LED_CMD_POWER_OFF		127
#define LED_CMD_ERR					111
#define LED_CMD_WARN				95
#define LED_CMD_SELFPOWER		32
#define LED_CMD_POWER_ON		31
#define LED_CMD_KBRD				15


void indicator_led_init();
void led_indicate(uint8_t type);
void led_indicate_on_always();
void led_off(uint8_t type);


#endif //INDICATOR_LED_H_
