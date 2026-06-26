/*
 * calib_task.c
 *
 *  Created on: 2026. 6. 24.
 *      Author: RCY
 */

#include "calib_task.h"

#include "utils.h"
#include "msg_types.h"

#include "color.h"
#include "color_sense.h"
#include "flash.h"
#include "sensor_task.h"


/* ---- 튜닝 파라미터 ---- */
#define CALIB_STEP_PER_COLOR	740U	// 색과 색 사이 전진 스텝 수
#define CALIB_SAMPLE_COUNT		6U		// 정지 후 평균낼 샘플 수
#define CALIB_SAMPLE_GAP_MS		40U		// 샘플 간 간격 (>= 측정시간 35ms)
#define CALIB_SETTLE_MS			150U	// 정지 후 흔들림 안정 대기
#define CALIB_START_DELAY_MS	1000U	// START 입력 후 시작음 -> 1초 뒤 시퀀스 시작

/* 캘리브레이션 색 순서는 color_t enum 순서와 동일:
 * GRAY -> RED -> ORANGE -> YELLOW -> GREEN -> BLUE -> PURPLE
 * -> LIGHT_GREEN -> SKY_BLUE -> PINK -> BLACK -> WHITE  (총 COLOR_COUNT개) */

typedef enum
{
	CALIB_ST_IDLE = 0,	// 대기: ARM -> ARMED
	CALIB_ST_ARMED,		// 진입(호흡): START -> 시퀀스 시작
	CALIB_ST_RUNNING,	// 시퀀스 실행 중
}calib_state_t;

/* 누적 버퍼: 스택 절약을 위해 static (좌/우 각 색의 평균 raw) */
static rgb_raw_t s_buf_left[COLOR_COUNT];
static rgb_raw_t s_buf_right[COLOR_COUNT];


/* ui_task로 단방향 알림 (LED/부저는 ui가 처리) */
static void ui_notify(ui_evt_type_t type, color_t color)
{
	ui_msg_t m = { .type = type, .calib_color = color };
	osMessageQueuePut(ui_queue, &m, 0, 0);
}

/* control_task에 "steps 전진" 명령 후 MOVE_DONE 회신까지 대기 (blocking) */
static void request_move_and_wait(uint32_t steps)
{
	ctrl_msg_t cmd = { .cmd = CTRL_CMD_CALIB_MOVE, .steps = steps };
	osMessageQueuePut(ctrl_queue, &cmd, 0, 0);

	/* MOVE_DONE만 기다리고, 그 사이 들어온 다른 입력(START 등)은 무시 */
	calib_msg_t m;
	do
	{
		if (osMessageQueueGet(calib_queue, &m, NULL, osWaitForever) != osOK)
		{
			continue;
		}
	} while (m.evt != CALIB_EVT_MOVE_DONE);
}


/* 수집한 좌/우 버퍼를 flash에 한꺼번에 저장 후 RAM 기준테이블 재로드 */
static void save_all(void)
{
	flash_erase_color_table(BH1749_ADDR_LEFT);
	flash_erase_color_table(BH1749_ADDR_RIGHT);

	for (uint8_t i = 0; i < COLOR_COUNT; i++)
	{
		reference_entry_t eL = { .raw = s_buf_left[i],  .color = (color_t)i, .offset = 0 };
		reference_entry_t eR = { .raw = s_buf_right[i], .color = (color_t)i, .offset = 0 };

		flash_write_color_reference(BH1749_ADDR_LEFT,  i, eL);
		flash_write_color_reference(BH1749_ADDR_RIGHT, i, eR);
	}

	/* sensor_task가 멈춰 있는 동안 안전하게 RAM 테이블 갱신 */
	color_sense_load_references();
}

/* 한 센서(addr)에서 N회 읽어 평균 raw 산출 (calib가 데이터 수집의 주체) */
static void request_color_sensor(color_t index)
{
	sensor_msg_t msg = { .cmd = SENSOR_CMD_CALIB_SAMPLE };
	osMessageQueuePut(sensor_queue, &msg, 0, 0);

	calib_msg_t m;
	do
	{
		if (osMessageQueueGet(calib_queue, &m, NULL, osWaitForever) != osOK)
		{
			continue;
		}
	} while (m.evt != CALIB_EVT_SAMPLE_DONE);

	s_buf_left[index] = m.left;
	s_buf_right[index] = m.right;
}

/* 캘리브레이션 본 시퀀스: calib는 "조율"만 하고 실제 동작은 각 태스크가 수행 */
static void run_sequence(void)
{
	/* 평소 color 폴링 멈춤 (calib가 I2C 단독 사용) */
	sensor_color_pause(true);

	for (color_t i = COLOR_GRAY; i < COLOR_COUNT; i++)
	{
		/* ui_task: 지금 캘리브할 색을 LED로 표시 */
		ui_notify(UI_EVT_CALIB_SHOW_COLOR, (color_t)i);

		/* 정지 안정화 후 좌/우 센서 샘플 수집 */
//		osDelay(CALIB_SETTLE_MS);
		request_color_sensor((color_t)i);

		/* control_task: 740스텝 전진 -> 완료 회신 대기 */
		if (i != COLOR_WHITE)
			request_move_and_wait(CALIB_STEP_PER_COLOR);
	}

	/* 한꺼번에 저장 */
	save_all();

	/* ui_task: 완료음 + 소등, 평소 동작 복귀 */
	ui_notify(UI_EVT_CALIB_DONE, COLOR_BLACK);
	sensor_color_pause(false);
}

/* run_sequence 동안 큐에 쌓인 잔여 입력 폐기 (끝나고 재진입 방지) */
static void drain_queue(void)
{
	calib_msg_t m;
	while (osMessageQueueGet(calib_queue, &m, NULL, 0) == osOK)
	{
		/* discard */
	}
}

void calib_task(void *argument)
{
	(void)argument;

	calib_state_t state = CALIB_ST_IDLE;

	for (;;)
	{
		calib_msg_t msg;
		if (osMessageQueueGet(calib_queue, &msg, NULL, osWaitForever) != osOK)
		{
			continue;
		}

		switch (state)
		{
			case CALIB_ST_IDLE:
				/* ARM 입력에만 반응 -> 대기 진입 (라이트그린 호흡) */
				if (msg.evt == CALIB_CMD_ARM)
				{
					state = CALIB_ST_ARMED;
					ui_notify(UI_EVT_CALIB_ARMED, COLOR_LIGHT_GREEN);
				}
				break;

			case CALIB_ST_ARMED:
				/* START 입력 -> 시작음 -> 1초 뒤 시퀀스 실행 */
				if (msg.evt == CALIB_CMD_START)
				{
					state = CALIB_ST_RUNNING;
					ui_notify(UI_EVT_CALIB_START, COLOR_BLACK);
					osDelay(CALIB_START_DELAY_MS);

					run_sequence();
//
					drain_queue();
					state = CALIB_ST_IDLE;
				}
				break;

			case CALIB_ST_RUNNING:

				break;
			default:
				/* 실행 중 입력 무시 */
				break;
		}
	}
}
