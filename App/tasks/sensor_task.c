/*
 * sensor_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#include "sensor_task.h"

#include "msg_types.h"
#include "utils.h"

#include "ir.h"
#include "vbat_adc.h"
#include "battery_monitor.h"

#include "ir_monitor.h"


#define BATTERY_EVAL_DIV	100U
#define IR_EVAL_DIV 5





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

static void publish_ir(void)
{
	uint16_t adc = ir_read_adc();
	ir_result_t res;

	ir_monitor_update(adc, &res);

	ui_msg_t ir_msg;
	ir_msg.type = UI_EVT_IR_DETECTED;
	ir_msg.ir_detected = res.ir_detected;

	osMessageQueuePut(ui_queue, &ir_msg, 0, 0);
}




void sensor_task(void *argument)
{
	(void)argument;

	ir_init();

	uint32_t tick 		= tick_now();
	uint32_t bat_div	= 0;
	uint32_t ir_div		= 0;

	for (;;)
	{
		if (++ir_div > IR_EVAL_DIV)
		{
			ir_div = 0;
			publish_ir();
		}

		if (++bat_div > BATTERY_EVAL_DIV)
		{
			bat_div = 0;
			publish_battery();
		}

		tick += 20;
		sleep_until(tick);
	}
}
