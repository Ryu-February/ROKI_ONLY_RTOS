/*
 * stby_monitor.c
 *
 *  Created on: 2026. 6. 16.
 *      Author: RCY
 */
#include "stby_monitor.h"

#include "utils.h"

#include "btn.h"

#include "msg_types.h"

#include "lp_stby.h"

// 내부 상태
static bool		s_last_pressed   = false;	// 마지막 샘플(raw)
static uint16_t s_stable_ms      = 0;   		// 같은 raw가 지속된 ms
static bool 	s_stable_pressed = false;   	// 디바운스 통과 상태(1: not pressed)
static uint32_t s_press_ms       = 0;   		// 눌림 지속 ms
static bool		s_off_enabled	 = false;
static uint32_t	s_system_ms	 = false;



void stby_monitor_update(bool pressed)
{
	if(++s_system_ms > 400)		//400 * 5ms = 2sec
		s_off_enabled = true;

	if (pressed)
	{
		// --- 디바운스 갱신 ---
		if (pressed == s_last_pressed)
		{
			if (s_stable_ms < 0xFFFF)
			{
				s_stable_ms++;
			}
		}
		else
		{
			s_stable_ms   = 0;
			s_last_pressed = pressed;
		}

		if (s_stable_ms == LP_STBY_DEBOUNCE_MS)
		{
			s_stable_pressed = pressed;     // 여기서 비로소 true/false로 “안정된 상태” 확정
		}
	}

	// --- 상태머신 ---
	static enum { IDLE, HOLDING, ARMED } s_state = IDLE;
	static uint16_t s_bz_wait_ms = 0;

	switch (s_state)
	{
		case IDLE:
		{
			if (!s_off_enabled)
				return;

			if (s_stable_pressed)   // 눌림(디바운스 통과)
			{
				if (s_press_ms < 0x7FFFFFFF)
				{
					s_press_ms++;
				}

				if (s_press_ms >= LP_STBY_HOLD_MS)  // 0.5초 이상 눌림
				{
					s_state = HOLDING;
//					s_power_off = true;				//color_sensor 마지막에 잘못 인식하는 거 방지하려고 함

					s_bz_wait_ms = 172;				//860ms = 172 * 5ms
					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);//모터 동작 중에 끄면 모터 돌아가면서 끝나서 추가함(모터드라이버 sleep핀)

//					ui_msg_t msg;
//					msg.type = UI_EVT_STBY_ENTERED;
//					osMessageQueuePut(ui_queue, &msg, 0, 0);
				}
			}
			else
			{
				s_press_ms = 0;
			}
		} break;

		case HOLDING:
		{
			if(--s_bz_wait_ms == 0)//when the buzzer sounds end, it gonna be entered standby mode
			{
				s_state = ARMED;
			}
		} break;

		case ARMED:
		{
//			enter_standby_safe();
		} break;
	}
}

static bool wkup1_arm_for_pa0(void)
{
    // 0) Disable
    CLEAR_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);

    // 1) 소스 선택: WUCR3.WUSEL1을 PA0에 맞는 값으로 설정
//    MODIFY_REG(PWR->WUCR3, PWR_WUCR3_WUSEL1_Msk,
//               (WKUP1_SEL_FOR_PA0 << PWR_WUCR3_WUSEL1_Pos));

    // 2) 플래그 클리어
    WRITE_REG(PWR->WUSR, PWR_WUSR_WUF1);

    // 3) Enable
    SET_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);

    // 4) Enable 직후 플래그 체크 → 활성로 보이면 취소
    if (READ_BIT(PWR->WUSR, PWR_WUSR_WUF1)) {
        CLEAR_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);
        WRITE_REG(PWR->WUSR, PWR_WUSR_WUF1);
        return false;
    }
    return true;
}

void enter_standby_safe(void)
{
    // 릴리즈(High) 최종 소프트 체크
//    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) != GPIO_PIN_SET)
//    	return;

    lp_stby_prepare_before();

    if (!wkup1_arm_for_pa0())
    	return;     // 활성로 보이면 진입 취소

    __disable_irq();
    HAL_SuspendTick();
//    HAL_PWR_EnterSTANDBYMode();
    HAL_PWREx_EnterSHUTDOWNMode();
    while (1) { }
}
