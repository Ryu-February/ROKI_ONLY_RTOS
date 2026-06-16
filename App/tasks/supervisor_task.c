/*
 * supervisor_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#include "supervisor_task.h"


#include "utils.h"


extern IWDG_HandleTypeDef hiwdg;


void supervisor_task(void *argument)
{
	uint32_t tick = tick_now();

	for (;;)
	{
		HAL_IWDG_Refresh(&hiwdg);

		tick += 1000;
		sleep_until(tick);
	}
}
