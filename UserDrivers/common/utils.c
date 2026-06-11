/*
 * utils.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */


#include "utils.h"





uint32_t tick_now(void)
{
	return osKernelGetTickCount();
}


void sleep_until(uint32_t tick)
{
	osDelayUntil(tick);
}
