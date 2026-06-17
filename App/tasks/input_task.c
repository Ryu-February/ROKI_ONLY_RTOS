/*
 * input_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#include "utils.h"

#include "msg_types.h"




void input_task(void *argument)
{
	(void)argument;

	btn_init();

	uint32_t tick = tick_now();

	for (;;)
	{
		btn_update_1ms();

		btn_id_t b;
		if (btn_pop_any_press(&b))
		{
			ui_msg_t msg;

			msg.btn = b;
			msg.type = UI_EVT_BTN_PRESSED;
			osMessageQueuePut(ui_queue, &msg, 0, 0);
		}

		if (btn_pop_long_press(BTN_POWER, 500))
		{
			spvr_msg_t msg = { .type = SPVR_EVT_IDLE, .stby_pressed = true};
			osMessageQueuePut(spvr_queue, &msg, 0, 0);
		}

		tick += 1;
		sleep_until(tick);
	}
}
