/*
 * bootloader.c
 *
 *  Created on: 2026. 2. 9.
 *      Author: RCY
 */


#include "bootloader.h"

#include "utils.h"

#include "btn.h"
#include "buzzer.h"
#include "rgb.h"


static void boot_write_bkp_data(uint32_t bkp_index, uint32_t data)
{
	uint32_t backup_register = 0U;

	/* Enable the write access to backup domain */
	SET_BIT(PWR->DBPR, PWR_DBPR_DBP);

	backup_register = (uint32_t)(TAMP_BASE + 0x100U);
	backup_register += (bkp_index * 4U);

	*(__IO uint32_t *) backup_register = data;

	/* Disable the write access to backup domain */
	CLEAR_BIT(PWR->DBPR, PWR_DBPR_DBP);
	NVIC_SystemReset();
}


void boot_enter_bootloader(void)
{
	uint32_t bkp_index = RTC_BKP_DR1;
	uint32_t flag      = 0x00000001U;

	boot_write_bkp_data(bkp_index, flag);
}


typedef enum
{
	BOOT_SEQ_IDLE = 0,
	BOOT_SEQ_WAIT_HOLD,
	BOOT_SEQ_WAIT_BUZZER,
	BOOT_SEQ_LED_ON,
	BOOT_SEQ_LED_OFF
} boot_seq_state_t;

static boot_seq_state_t s_boot_seq_state = BOOT_SEQ_IDLE;
static uint32_t         s_boot_seq_start_ms = 0;
static uint32_t         s_boot_led_ms = 0;
static uint8_t          s_boot_led_count = 0;

#define BOOT_COMBO_HOLD_MS		2000U //	// 3버튼 동시 길게-누름 시간 (400U * 5ms)

/* delete + execute + resume 3버튼 동시 눌림 (btn 디바운스 상태 사용) */
static bool boot_combo_down(void)
{
	return btn_is_pressed(BTN_DELETE)  &&
	       btn_is_pressed(BTN_EXECUTE) &&
	       btn_is_pressed(BTN_RESUME);
}


void bootloader_combo_tick(void)
{
	uint32_t tick 	= tick_now();
	bool     combo  = boot_combo_down();

	switch (s_boot_seq_state)
	{
		case BOOT_SEQ_IDLE:
		{
			if (combo)
			{
				s_boot_seq_state    = BOOT_SEQ_WAIT_HOLD;
				s_boot_seq_start_ms = tick;
			}
		} break;

		case BOOT_SEQ_WAIT_HOLD:
		{
			if (!combo)
			{
				s_boot_seq_state = BOOT_SEQ_IDLE;	// 도중에 하나라도 떼면 취소
			}
			else if ((uint32_t)(tick - s_boot_seq_start_ms) >= BOOT_COMBO_HOLD_MS)
			{
				buzzer_play_bootloader_enter();
				s_boot_seq_state = BOOT_SEQ_WAIT_BUZZER;
			}
		} break;

		case BOOT_SEQ_WAIT_BUZZER:
		{
			if (!buzzer_is_busy())
			{
				s_boot_led_count = 0;
				rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_BLUE);
				s_boot_led_ms = tick;
				s_boot_seq_state = BOOT_SEQ_LED_ON;
			}
		} break;

		case BOOT_SEQ_LED_ON:
		{
			if ((uint32_t)(tick - s_boot_led_ms) >= 100U)
			{
				rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_BLACK);
				s_boot_led_ms = tick;
				s_boot_led_count++;
				if (s_boot_led_count >= 3U)
				{
					boot_enter_bootloader();		// BKP 플래그 set 후 시스템 리셋(복귀 없음)
				}
				else
				{
					s_boot_seq_state = BOOT_SEQ_LED_OFF;
				}
			}
		} break;

		case BOOT_SEQ_LED_OFF:
		{
			if ((uint32_t)(tick - s_boot_led_ms) >= 100U)
			{
				rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_RED);
				s_boot_led_ms = tick;
				s_boot_seq_state = BOOT_SEQ_LED_ON;
			}
		} break;
	}
}


bool bootloader_is_active(void)
{
	return (s_boot_seq_state != BOOT_SEQ_IDLE);
}



