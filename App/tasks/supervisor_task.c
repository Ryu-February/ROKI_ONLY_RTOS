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


#define SUPERVISOR_POLL_MS	5U							//루프 주기(ms)
#define IWDG_REFRESH_DIV	(1000U / SUPERVISOR_POLL_MS)	//~1초마다 IWDG 갱신


extern IWDG_HandleTypeDef hiwdg;




void supervisor_task(void *argument)
{
	(void)argument;

	lp_stby_init();

	uint32_t tick     = tick_now();
	uint32_t iwdg_div = 0;

	for (;;)
	{
		/* 전원 버튼/유휴 타임아웃: 5ms 단위로 카운트.
		   0.5s 홀드 확정 시 내부에서 STANDBY 진입(복귀 없음). */
		lp_stby_on_5ms();

		/* 부트로더 진입 콤보(delete+execute+resume 2s 홀드).
		   tim6_get_ms() 타임스탬프 기반이라 폴링 주기와 무관. */
		bootloader_combo_tick();

		if (++iwdg_div >= IWDG_REFRESH_DIV)
		{
			iwdg_div = 0;
			HAL_IWDG_Refresh(&hiwdg);
		}

		tick += SUPERVISOR_POLL_MS;
		sleep_until(tick);
	}
}
