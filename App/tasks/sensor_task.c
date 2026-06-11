/*
 * sensor_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#include "sensor_task.h"

#include "msg_types.h"
#include "utils.h"

#include "vbat_adc.h"
#include "battery_monitor.h"


#define BATTERY_EVAL_DIV	100U

static void publish_battery(void)
{
	battery_state_t s_batt;
	battery_result_t res;

	bool running = false;
	uint16_t adc = vbat_read_adc();

	battery_monitor_update(&s_batt, adc, running, &res);

	ui_msg_t vbat_msg;
	vbat_msg.type = UI_EVT_BAT_INDICATE;
	vbat_msg.bat_low = res.low;

	osMessageQueuePut(ui_queue, &vbat_msg, 0, 0);
}




void sensor_task(void *argument)
{
	(void)argument;

	uint32_t tick 		= tick_now();
	uint32_t bat_div	= 0;

	for (;;)
	{
		if (++bat_div > BATTERY_EVAL_DIV)
		{
			bat_div = 0;
			publish_battery();
		}

		tick += 20;
		sleep_until(tick);
	}
}
