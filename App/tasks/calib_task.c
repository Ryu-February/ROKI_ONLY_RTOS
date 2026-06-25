/*
 * calib_task.c
 *
 *  Created on: 2026. 6. 24.
 *      Author: RCY
 */

#include "calib_task.h"

#include "utils.h"
#include "msg_types.h"




void calib_task(void *argument)
{
	(void)argument;

	uint32_t tick = tick_now();

	for (;;)
	{
		calib_msg_t msg = { .type = CALIB_EVT_IDLE };
		if (osMessageQueueGet(calib_queue, &msg, 0, osWaitForever) != osOK)
		{
			continue;
		}

		if (msg.type == CALIB_EVT_IDLE && msg.pressed)
		{
			msg.type = CALIB_EVT_ARMED;
		}

		switch (msg.type)
		{
			case CALIB_EVT_IDLE:
				break;
			case CALIB_EVT_ARMED:
			{
				ui_msg_t ui_msg;
				ui_msg.type = UI_EVT_CALIB_ARMED;
				osMessageQueuePut(ui_queue, &ui_msg, 0, 0);
				break;
			}
			default:
				break;
		}
	}
}
