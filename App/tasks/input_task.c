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
		ui_msg_t msg;

		btn_update_1ms();

		btn_id_t b;
		if (btn_pop_any_press(&b))
		{
			msg.btn = b;
			msg.type = UI_EVT_BTN_PRESSED;

			osMessageQueuePut(ui_queue, &msg, 0, osWaitForever);
		}

		tick += 1;
		sleep_until(tick);
	}
}
