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

#define CALIB_SAMPLE_COUNT	6U		// 캘리브 샘플 평균 횟수
#define CALIB_SAMPLE_GAP_MS	40U		// 샘플 간 간격 (>= 측정시간 35ms)


static color_sense_t s_color;		//카드 명령 누적 상태

static volatile bool s_color_paused = false;	//캘리브레이션 중 color 폴링 정지

void sensor_color_pause(bool on)
{
	s_color_paused = on;
}

/* 한 센서(addr)에서 N회 읽어 평균 raw 산출 */
static void calib_sample_side(uint8_t addr, rgb_raw_t *out)
{
	uint32_t r = 0, g = 0, b = 0;

	for (uint32_t i = 0; i < CALIB_SAMPLE_COUNT; i++)
	{
		bh1749_color_data_t c = bh1749_read_rgbir(addr);
		r += c.red;
		g += c.green;
		b += c.blue;
		osDelay(CALIB_SAMPLE_GAP_MS);
	}

	out->red_raw   = (uint16_t)(r / CALIB_SAMPLE_COUNT);
	out->green_raw = (uint16_t)(g / CALIB_SAMPLE_COUNT);
	out->blue_raw  = (uint16_t)(b / CALIB_SAMPLE_COUNT);
}

/* calib_task의 SAMPLE 명령 처리: 좌/우 평균 raw를 calib_queue로 회신.
 * 반환값: 긴 블로킹(샘플) 수행 여부 (loop cadence 재동기화용) */
static bool handle_sensor_cmd(const sensor_msg_t *m)
{
	switch (m->cmd)
	{
		case SENSOR_CMD_CALIB_SAMPLE:
		{
			calib_msg_t done = { .evt = CALIB_EVT_SAMPLE_DONE };
			calib_sample_side(BH1749_ADDR_LEFT,  &done.left);
			calib_sample_side(BH1749_ADDR_RIGHT, &done.right);
			osMessageQueuePut(calib_queue, &done, 0, 0);
			return true;
		}
		default:
			return false;
	}
}



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



static void publish_color(void)
{

	sensor_msg_t msg;
	if (osMessageQueueGet(sensor_queue, &msg, 0, 0) == osOK)
	{
		handle_sensor_cmd(&msg);//캘리브레이션 중엔 calib_task가 I2C 단독 사용
		return;
	}

	bh1749_color_data_t l = bh1749_read_rgbir(BH1749_ADDR_LEFT);
	bh1749_color_data_t r = bh1749_read_rgbir(BH1749_ADDR_RIGHT);

	color_sense_result_t res;
	color_sense_update(&s_color, &l, &r, &res);

	/* (1) 현재 인식 중인 색: 매 호출 전역 갱신 (PC 라이브 표시용) */
	sensor_state_update_color(res.cur_color);

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
