/*
 * ui_feedback.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */


#include "led.h"
#include "rgb.h"
#include "rgb_effect.h"
#include "buzzer.h"

#include "ui_feedback.h"


void ui_feedback_init(void)
{
	led_init();
	rgb_init();
	rgb_effect_init();
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

static color_t color_by_card(color_mode_t cmd)
{
    switch (cmd)
    {
        case MODE_FORWARD:     return COLOR_GREEN;
        case MODE_BACKWARD:    return COLOR_RED;
        case MODE_LEFT:        return COLOR_BLUE;
        case MODE_RIGHT:       return COLOR_YELLOW;
        case MODE_REPEAT_ONCE: return COLOR_PURPLE;
        case MODE_REPEAT_TWICE:return COLOR_PINK;
        case MODE_REPEAT_THIRD:return COLOR_SKY_BLUE;
        default:               return COLOR_BLACK;
    }
}


void ui_feedback_btn_press_start(btn_id_t btn)
{
	// 1. LED 피드백
	color_t c = color_by_button(btn);
//	rgb_set_color(RGB_ZONE_V_SHAPE, c);

	// 2. 부저 피드백
//	buzzer_by_button(btn);
}

void ui_feedback_btn_press_timeout(void)
{
	rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_BLACK);
}


static void led_set(led_ch_t ch, bool state)
{
    state ? led_on(ch) : led_off(ch);
}

void ui_feedback_indicate_battery(bool low)
{
	// low 상태에 따라 각 LED의 ON/OFF 상태를 직접 주입
	led_set(LED_POWER_STAT_O, low);  // low(true)면 켜짐(true)
	led_set(LED_POWER_STAT_W, !low); // low(true)면 꺼짐(false)
}

static void ir_set(bool state)
{
    state ? rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_RED) : rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_BLACK);
}


void ui_feedback_on_obstacle(uint16_t detected)
{
	ir_set(detected);
}


void ui_feedback_indicate_card(color_mode_t cmd)
{
//	rgb_set_color(RGB_ZONE_V_SHAPE, color_by_card(cmd));
	buzzer_play_dir_click_soft();
}

void ui_feedback_disable(void)
{
	buzzer_play_space_exit();
	rgb_effect_enable_gpio_sync(true);
	rgb_effect_start(RGB_FX_SHUTDOWN_FADE, COLOR_WHITE);
	osDelay(1300);
}

void ui_feedback_power_on(void)
{
	buzzer_play_space_enter();

	rgb_effect_enable_gpio_sync(true);
	rgb_effect_start(RGB_FX_BOOT_BREATH, COLOR_WHITE);
//	rgb_effect_start(RGB_FX_SHUTDOWN_FADE, COLOR_WHITE);

	osDelay(1300);
}



