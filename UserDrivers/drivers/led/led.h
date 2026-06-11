/*
 * led.h
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */

#ifndef LED_LED_H_
#define LED_LED_H_


#include "def.h"



typedef enum
{
	LED_POWER_STAT_W 	= 0,
	LED_POWER_STAT_O 	= 1,
	LED_W_CONTROL		= 2,
	LED_CH_COUNT
}led_ch_t;


void led_init(void);
void led_on(led_ch_t ch);
void led_off(led_ch_t ch);
void led_toggle(led_ch_t ch);
void led_write(led_ch_t ch, bool on);
bool led_is_on(led_ch_t ch);
void led_boot_blink(void);



#endif /* LED_LED_H_ */
