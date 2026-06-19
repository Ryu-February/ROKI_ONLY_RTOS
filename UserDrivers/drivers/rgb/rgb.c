/*
 * rgb.c
 *
 *  Created on: Sep 23, 2025
 *      Author: RCY
 */


#include "rgb.h"
#include "led.h"

#include "bsp_isr.h"
#include "board.h"

#include "rgb_effect.h"


const rgb_led_t led_map[COLOR_COUNT] =
{
		[COLOR_RED]         = { 255,   0,   0 },
		[COLOR_ORANGE]      = { 255,  60,   0 },
		[COLOR_YELLOW]      = { 255, 255,   0 },
		[COLOR_GREEN]       = {   0, 255,   0 },
		[COLOR_BLUE]        = {   0,   0, 255 },
		[COLOR_PURPLE]      = { 160,  32, 240 },  // or VIOLET
		[COLOR_LIGHT_GREEN] = {  60, 220,  20 },
		[COLOR_SKY_BLUE]    = {  70, 200, 255 },  // LIGHT_BLUE or CYAN mix
		[COLOR_PINK]        = { 255,  60,  70 },
		[COLOR_BLACK]       = {   0,   0,   0 },
		[COLOR_WHITE]       = { 255, 255, 255 },
		[COLOR_GRAY]        = { 128, 128, 128 }
};


/* ───────────────── 핀 매핑 (Active-Low: LOW=ON) ─────────────────
 * V_SHAPE : PC0(B), PC1(R), PC2(G)
 * EYES    : PC10(B), PC11(R), PC12(G)
 */



/* 채널 인덱스 */
enum { CH_R = 0, CH_G = 1, CH_B = 2 };


/* 듀티 버퍼: [ZONE][CH] */
static volatile uint8_t duty[RGB_ZONE_COUNT][3] = {0};

/* 공용 PWM 카운터(0~254) */
static volatile uint8_t pwm_counter = 0;

/* Active-Low 보정된 핀 쓰기 */
static inline void write_pin_active_low(uint16_t pin, uint8_t on)
{
    HAL_GPIO_WritePin(RGB_PORT, pin, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}


/* 하나의 ZONE에 대해 세 채널을 즉시 반영 */
static inline void apply_zone(rgb_zone_t zone, uint8_t r_on, uint8_t g_on, uint8_t b_on)
{
    if (zone == RGB_ZONE_V_SHAPE)
    {
        write_pin_active_low(RGB_V_R_PIN, r_on);
        write_pin_active_low(RGB_V_G_PIN, g_on);
        write_pin_active_low(RGB_V_B_PIN, b_on);
    }
//    else	/* RGB_ZONE_EYES */
//    {
//        write_pin_active_low(E_R_PIN, r_on);
//        write_pin_active_low(E_G_PIN, g_on);
//        write_pin_active_low(E_B_PIN, b_on);
//    }
}

/* ───────────────── Public API ───────────────── */

void rgb_init(void)
{
	HAL_GPIO_WritePin(RGB_PORT,
						RGB_V_R_PIN | RGB_V_G_PIN | RGB_V_B_PIN, //|
//	                      E_R_PIN | E_G_PIN | E_B_PIN,
	                      GPIO_PIN_SET);

	/* 기본 듀티 0 */
	for (int Z = 0; Z < RGB_ZONE_COUNT; ++Z)
	{
		duty[Z][CH_R] = 0;
		duty[Z][CH_G] = 0;
		duty[Z][CH_B] = 0;
	}
	pwm_counter = 0;
}

void rgb_set_color(rgb_zone_t zone, color_t color)
{
	if(color >= COLOR_COUNT)
		return;

    const rgb_led_t *c = &led_map[color];

    duty[zone][CH_R] = c->r;
    duty[zone][CH_G] = c->g;
    duty[zone][CH_B] = c->b;
}

void rgb_set_rgb(rgb_zone_t zone, uint8_t r, uint8_t g, uint8_t b)
{
    duty[zone][CH_R] = r;
    duty[zone][CH_G] = g;
    duty[zone][CH_B] = b;
}


// 타이머 주기마다 호출(예: 타이머 주파수/255 = 200~1000Hz 영역)
void rgb_tick(void)
{
    pwm_counter++;

    // V_SHAPE
    {
        uint8_t r_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_V_SHAPE][CH_R]));
        uint8_t g_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_V_SHAPE][CH_G]));
        uint8_t b_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_V_SHAPE][CH_B]));
        apply_zone(RGB_ZONE_V_SHAPE, r_on, g_on, b_on);

        // 2. 캡슐화된 함수로 조건 검사 (전역 변수 없음!)
		if (rgb_effect_is_gpio_sync_active())
		{
			// 부팅 등 이펙트가 돌 때만 60µs 초고속 싱크 브리딩 구동!
			r_on ? led_on(LED_W_CONTROL) 	: led_off(LED_W_CONTROL);
			r_on ? led_on(LED_POWER_STAT_W) : led_off(LED_POWER_STAT_W);
		}
    }

//    // EYES
//    {
//        uint8_t r_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_EYES][CH_R]));
//        uint8_t g_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_EYES][CH_G]));
//        uint8_t b_on = (pwm_counter > (uint8_t)(255 - duty[RGB_ZONE_EYES][CH_B]));
//        apply_zone(RGB_ZONE_EYES, r_on, g_on, b_on);
//    }
}


const rgb_led_t *rgb_get_led_map(color_t color)
{
    if (color >= COLOR_COUNT) return &led_map[COLOR_BLACK];
    return &led_map[color];
}
