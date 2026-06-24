/*
 * control_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#include "control_task.h"


#include "msg_types.h"
#include "stepper.h"
#include "sensor_state.h"


void control_task(void *argument)
{
	(void)argument;

	step_init_all();

	uint32_t tick = tick_now();

	for (;;)
	{
		sensor_snapshot_t s;
		sensor_state_get(&s);

//		if (s.ir_detected)
//		{
//			step_drive(OP_REVERSE);
//		}
//		else
//		{
//			step_drive(OP_STOP);
//		}

		tick += 20;
		sleep_until(tick);
	}
}
