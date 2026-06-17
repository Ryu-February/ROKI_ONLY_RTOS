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

#include "color.h"
#include "color_sense.h"


#define BATTERY_EVAL_DIV	100U
#define IR_EVAL_DIV 		5U
#define COLOR_EVAL_DIV		2U		//20ms * 2 = 40ms (color_sensor measurement time: 35ms )


static color_sense_t s_color;		//카드 명령 누적 상태



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

static void battery_init(void)
{
	battery_state_t s_batt = { 0 };
	battery_result_t res;

	bool running = false;
	uint16_t adc = vbat_read_adc();

	battery_monitor_update(&s_batt, adc, running, &res);
	sensor_state_update_bat(res.low, res.pct);

	osMessageQueuePut(ui_queue, &(ui_msg_t){ .type = UI_EVT_BAT_INDICATE, .bat_low = res.low }, 0, 0);
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


volatile uint16_t red = 0;
volatile uint16_t green = 0;
volatile uint16_t blue = 0;

color_t d_c = 0;

static void publish_color(void)
{
	bh1749_color_data_t l = bh1749_read_rgbir(BH1749_ADDR_LEFT);
	bh1749_color_data_t r = bh1749_read_rgbir(BH1749_ADDR_RIGHT);

	/* 디버그 워치용 */
	red   = l.red;
	green = l.green;
	blue  = l.blue;

	color_sense_result_t res;
	color_sense_update(&s_color, &l, &r, &res);

	/* (1) 현재 인식 중인 색: 매 호출 전역 갱신 (PC 라이브 표시용) */
	sensor_state_update_color(res.cur_color);
	d_c = res.cur_color;

	/* (2) 확정 카드 명령: 새 카드일 때만 전역 갱신 + 이벤트 */
	if (res.card_inserted)
	{
		if (sensor_state_update_card(res.inserted_cmd, res.count))
		{
			ui_msg_t msg =
			{
				.type       = UI_EVT_CARD_INSERTED,
				.card       = res.inserted_cmd,
				.card_count = res.count,
			};
			osMessageQueuePut(ui_queue, &msg, 0, 0);
		}
	}
}


void sensor_task(void *argument)
{
	(void)argument;

	ir_init();
	color_init();
	color_sense_init(&s_color);
	battery_init();

	uint32_t tick 		= tick_now();

	uint32_t ir_div		= 0;
	uint32_t bat_div	= 0;
	uint32_t color_div  = 0;

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

		if (++color_div > COLOR_EVAL_DIV)	//40ms
		{
			color_div = 0;
			publish_color();
		}

		tick += 20;
		sleep_until(tick);
	}
}
