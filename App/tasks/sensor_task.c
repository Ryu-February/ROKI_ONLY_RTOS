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
#include "sensor_state.h"


#define BATTERY_EVAL_DIV	100U
#define IR_EVAL_DIV 5



static void sensor_dispatch(const sensor_evt_t *e)
{
	switch (e->kind)
	{
		case SENS_IR:
		{
			ui_msg_t 	u = { .type = UI_EVT_IR_DETECTED, .ir_detected = e->ir_detected };
			ctrl_msg_t	c = { .ir_detected = e->ir_detected };

			osMessageQueuePut(ui_queue,   &u, 0, 0);
			osMessageQueuePut(ctrl_queue, &c, 0, 0);
			break;
		}
		case SENS_BAT:
		{
			ui_msg_t 	u = { .type = UI_EVT_BAT_INDICATE, .bat_low = e->bat_low };
			osMessageQueuePut(ui_queue, &u, 0, 0);
			break;
		}
	}
}

static void publish_battery(void)
{
	battery_state_t s_batt;
	battery_result_t res;

	bool running = false;
	uint16_t adc = vbat_read_adc();

	battery_monitor_update(&s_batt, adc, running, &res);

	if (sensor_state_update_bat(res.low, res.pct))
	{
		osMessageQueuePut(ui_queue, &(ui_msg_t){ .type = UI_EVT_BAT_INDICATE }, 0, 0);
	}
}


static void publish_ir(void)
{
	uint16_t adc = ir_read_adc();
	ir_result_t res;

	ir_monitor_update(adc, &res);

	if (sensor_state_update_ir(res.ir_detected))
	{
		osMessageQueuePut(ui_queue, &(ui_msg_t){ .type = UI_EVT_IR_DETECTED}, 0, 0);
	}
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
		if (++ir_div > IR_EVAL_DIV)			//100ms
		{
			ir_div = 0;
			publish_ir();
		}

		if (++bat_div > BATTERY_EVAL_DIV)	//2초
		{
			bat_div = 0;
			publish_battery();
		}

		tick += 20;
		sleep_until(tick);
	}
}
