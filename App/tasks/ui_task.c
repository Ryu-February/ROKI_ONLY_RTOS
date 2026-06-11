/*
 * ui_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */


#include "ui_task.h"

#include "utils.h"

#include "ui_feedback.h"



void ui_task(void *argument)
{
	(void)argument;

	ui_feedback_init();

	uint32_t tick = tick_now();

	for (;;)
	{
//		led_toggle(LED_W_CONTROL);

		tick += 500;
		sleep_until(tick);
	}
}
