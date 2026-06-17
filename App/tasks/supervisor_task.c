/*
 * supervisor_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#include "supervisor_task.h"


#include "utils.h"

#include "lp_stby.h"
#include "bootloader.h"

#include "msg_types.h"

#include "stby_monitor.h"
#include "rgb.h"


#define SUPERVISOR_POLL_MS	5U							//루프 주기(ms)
#define IWDG_REFRESH_DIV	(1000U / SUPERVISOR_POLL_MS)	//~1초마다 IWDG 갱신


extern IWDG_HandleTypeDef hiwdg;

static osTimerId_t stby_enter_timer_id;


static void stby_enter_timer_callback(void *argument)
{
	(void)argument;

	spvr_msg_t msg = { .type = SPVR_EVT_STBY_ENTERED };

	osMessageQueuePut(spvr_queue, &msg, 0, 0);
}

void supervisor_task(void *argument)
{
	(void)argument;

	lp_stby_init();

	uint32_t tick     = tick_now();
	uint32_t iwdg_div = 0;

	stby_enter_timer_id = osTimerNew(stby_enter_timer_callback, osTimerOnce, NULL, NULL);

	for (;;)
	{
		if (++iwdg_div >= IWDG_REFRESH_DIV)
		{
			iwdg_div = 0;
			HAL_IWDG_Refresh(&hiwdg);
		}

		/* 전원 버튼/유휴 타임아웃: 5ms 단위로 카운트.
		   0.5s 홀드 확정 시 내부에서 STANDBY 진입(복귀 없음). */
		spvr_msg_t spvr_msg = { .type = SPVR_EVT_IDLE };
		if (osMessageQueueGet(spvr_queue, &spvr_msg, 0, 0) == osOK)
		{
			switch (spvr_msg.type)
			{
				case SPVR_EVT_IDLE:
					if (spvr_msg.stby_pressed)
					{
						ui_msg_t ui_msg = { .type = UI_EVT_STBY_ENTERED};	//ui_task에서 쓸 msg
						osMessageQueuePut(ui_queue, &ui_msg, 0, 0);

						osTimerStop(stby_enter_timer_id);
						osTimerStart(stby_enter_timer_id, 860);
					}
					break;
				case SPVR_EVT_STBY_ENTERED:
					enter_standby_safe();
					break;
				default:
					break;
			}
		}
		/* 부트로더 진입 콤보(delete+execute+resume 2s 홀드).
		   tim6_get_ms() 타임스탬프 기반이라 폴링 주기와 무관. */
		bootloader_combo_tick();

		tick += SUPERVISOR_POLL_MS;
		sleep_until(tick);
	}
}
