/*
 * ui_feedback.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */


#include "led.h"
#include "rgb.h"
#include "buzzer.h"

#include "ui_feedback.h"


void ui_feedback_init(void)
{
	led_init();
	rgb_init();
}

static color_t color_by_button(btn_id_t id)
{
	switch (id)
	{
		case BTN_EXECUTE:	return COLOR_GREEN;		break;
		case BTN_DELETE: 	return COLOR_YELLOW;	break;
		case BTN_RESUME: 	return COLOR_RED;		break;
		default:			return COLOR_BLACK;		break;
	}
}

static void buzzer_by_button(btn_id_t id)
{
	switch (id)
	{
		case BTN_EXECUTE:	buzzer_play_execute();	break;
		case BTN_DELETE: 	buzzer_evt_delete();	break;
		case BTN_RESUME: 	buzzer_play_resume();	break;
		default:			break;
	}
}


void ui_feedback_btn_press_start(btn_id_t btn)
{
	// 1. LED 피드백
	color_t c = color_by_button(btn);
	rgb_set_color(RGB_ZONE_V_SHAPE, c);

	// 2. 부저 피드백
	buzzer_by_button(btn);
}

void ui_feedback_btn_press_timeout(void)
{
	rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_BLACK);
}
