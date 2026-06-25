/*
 * input_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#include "utils.h"

#include "msg_types.h"




void input_task(void *argument)
{
	(void)argument;

	btn_init();

	uint32_t tick = tick_now();

	for (;;)
	{
		btn_update_1ms();

		btn_id_t b;
		if (btn_pop_any_press(&b))
		{
			ui_msg_t msg;

			msg.btn = b;
			msg.type = UI_EVT_BTN_PRESSED;
			osMessageQueuePut(ui_queue, &msg, 0, 0);

			/* 캘리브 대기(ARMED) 상태일 때만 calib_task가 START로 해석.
			 * 평소엔 calib_task가 상태로 걸러서 무시함. */
			calib_msg_t cm = { .evt = CALIB_CMD_START };
			osMessageQueuePut(calib_queue, &cm, 0, 0);
		}

		/* 부트로더 진입 콤보(delete+execute+resume 2s 홀드).
				   tim6_get_ms() 타임스탬프 기반이라 폴링 주기와 무관. */
		if (btn_pop_long_press(BTN_RESUME, 2000))
//				&& btn_pop_long_press(BTN_DELETE, 2000)
//				&& btn_pop_long_press(BTN_EXECUTE, 2000))
		{
			spvr_msg_t msg = { .type = SPVR_EVT_BOOTLOADER_IDLE };
			osMessageQueuePut(spvr_queue, &msg, 0, 0);
		}

		if (btn_pop_long_press(BTN_POWER, 500))
		{
			spvr_msg_t msg = { .type = SPVR_EVT_STBY_IDLE };
			osMessageQueuePut(spvr_queue, &msg, 0, 0);
		}

		if (btn_pop_long_press(BTN_EXECUTE, 2000))
		{
			calib_msg_t msg = { .evt = CALIB_CMD_ARM };
			osMessageQueuePut(calib_queue, &msg, 0, 0);
		}

		tick += 1;
		sleep_until(tick);
	}
}
