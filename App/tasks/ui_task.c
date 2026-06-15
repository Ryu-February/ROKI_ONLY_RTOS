/*
 * ui_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */


#include "ui_task.h"

#include "utils.h"
#include "msg_types.h"

#include "ui_feedback.h"

#include "sensor_state.h"

static osTimerId_t rgb_off_timer_id;



void rgb_off_timer_callback(void *argument)
{
	(void)argument;

	ui_msg_t msg = { .type = UI_EVT_RGB_TIMEOUT };

	osMessageQueuePut(ui_queue, &msg, 0, osWaitForever);
}


void ui_task(void *argument)
{
	(void)argument;

	ui_feedback_init();

	rgb_off_timer_id = osTimerNew(rgb_off_timer_callback, osTimerOnce, NULL, NULL);

	for (;;)
	{
		ui_msg_t msg;

		if (osMessageQueueGet(ui_queue, &msg, 0, osWaitForever) != osOK)
		{
			continue;
		}

		switch (msg.type)
		{
			case UI_EVT_BTN_PRESSED:
				ui_feedback_btn_press_start(msg.btn);
				osTimerStop(rgb_off_timer_id);
				osTimerStart(rgb_off_timer_id, 500);
				break;
			case UI_EVT_BAT_INDICATE:
				ui_feedback_indicate_battery(sensor_state_get_vbat_low());
				break;
			case UI_EVT_RGB_TIMEOUT:
				ui_feedback_btn_press_timeout();
				break;
			case UI_EVT_IR_DETECTED:
				ui_feedback_on_obstacle(sensor_state_get_ir());
				break;
			default:
				break;
		}
	}
}
