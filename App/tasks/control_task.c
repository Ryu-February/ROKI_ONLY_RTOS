/*
 * control_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#include "control_task.h"


#include "utils.h"
#include "msg_types.h"
#include "stepper.h"
#include "calib_motion.h"




void control_task(void *argument)
{
	(void)argument;

	step_init_all();

	for (;;)
	{
		ctrl_msg_t msg;
		if (osMessageQueueGet(ctrl_queue, &msg, NULL, osWaitForever) != osOK)
		{
			continue;
		}

		switch (msg.cmd)
		{
			case CTRL_CMD_CALIB_MOVE:
				/* calib 명령: steps 전진 후 완료 회신 */
				calib_move_forward_steps(msg.steps);
				osMessageQueuePut(calib_queue, &(calib_msg_t){ .evt = CALIB_EVT_MOVE_DONE }, 0, 0);
				break;

			case CTRL_CMD_OBSTACLE:
			default:
				/* 장애물(IR) 등 기타 명령은 추후 처리 */
				break;
		}
	}
}
