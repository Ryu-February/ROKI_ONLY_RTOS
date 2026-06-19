/*
 * rgb_effect.c
 *
 *  Created on: 2026. 6. 19.
 *      Author: RCY
 */


#include "rgb_effect.h"
#include "led.h"
/* ── 타이밍 상수 (60µs tick 기준) ──────────────────────────────
 *  BREATH_STEPS : brightness 0→255→0 한 사이클의 스텝 수
 *  BREATH_DIVIDER: tick 몇 번에 스텝 1 증가 (속도 조절)
 *
 *  예) DIVIDER=6 → 스텝 주기 = 60µs × 6 = 360µs
 *      사이클 = 360µs × 512스텝 ≒ 184ms  (너무 빠름)
 *      DIVIDER=20 → 60µs × 20 × 512 ≒ 614ms  (딱 좋음)
 * ─────────────────────────────────────────────────────────── */
#define BREATH_DIVIDER      80u        /* 속도 조절 여기서 */
#define BREATH_STEPS        256u       /* 0~255 */
#define BREATH_CYCLE        (BREATH_STEPS * 2u)   /* up + down = 512 */

/* COLOR_BLACK 스킵 — 나머지만 순환 */
static const color_t breath_seq[] =
{
    COLOR_RED,
    COLOR_ORANGE,
    COLOR_YELLOW,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_PURPLE,
    COLOR_LIGHT_GREEN,
    COLOR_SKY_BLUE,
    COLOR_PINK,
    COLOR_WHITE,
    COLOR_GRAY,
};
#define SEQ_COUNT   (sizeof(breath_seq) / sizeof(breath_seq[0]))

static volatile rgb_fx_mode_t s_fx_mode = RGB_FX_NONE;
static color_t s_target_color = COLOR_BLACK;
static uint32_t s_tick_acc = 0;
static uint32_t s_step = 0;
static bool s_fx_include_gpio = false; // 일반 LED 동기화 여부 플래그

/* ── 감마 보정 LUT (8-bit → 8-bit, perceived brightness) ─────
 *  없어도 되지만 있으면 훨씬 자연스럽게 숨쉬는 느낌
 *  gamma ≈ 2.2 근사
 * ─────────────────────────────────────────────────────────── */
static const uint8_t gamma_lut[256] =
{
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
      1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
      2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
      5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
     10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
     17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
     25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
     37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
     51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
     69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
     90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
    115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
    144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
    177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
    215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255,
};

void rgb_effect_init(void)
{
    s_fx_mode = RGB_FX_NONE;
    s_tick_acc = 0;
    s_step = 0;
}

// 원하는 시점에 효과를 트리거하는 함수 (UI Task 등에서 호출)
void rgb_effect_start(rgb_fx_mode_t mode, color_t target_color)
{
    s_tick_acc = 0;
    s_step = 0;
    s_target_color = target_color;
    s_fx_mode = mode; // 모드를 변경하는 순간 ISR에서 연출 시작
}

void rgb_effect_tick(void)
{
    if (s_fx_mode == RGB_FX_NONE) return;

    const rgb_led_t *c = rgb_get_led_map(s_target_color);
    uint8_t bright = 0;
    uint32_t divider = 40; // 기본 분주값

    switch (s_fx_mode)
    {
        case RGB_FX_BOOT_BREATH:
            // 60µs * 20 * 512스텝 = 약 614ms (우주 시동음 v2의 590ms와 매칭)
            divider = 40;
            break;

        case RGB_FX_SHUTDOWN_FADE:
            // 60µs * 18 * 512스텝 = 약 552ms (우주 종료음 v2의 510ms와 매칭)
            divider = 36;
            break;

        case RGB_FX_CARD_FLASH:
            // 카드 인식은 60µs * 5 * 512스텝 = 약 153ms 동안 아주 빠르게 슥- 켜졌다 슥- 꺼짐
            divider = 10;
            break;

        default:
            return;
    }

    /* 분주 처리 */
    if (++s_tick_acc < divider) return;
    s_tick_acc = 0;

    /* [핵심] 순수 삼각파 구현: 0 -> 255 -> 0 */
    if (s_step < 256) {
        bright = gamma_lut[s_step];               // 0번부터 255번까지 서서히 밝아짐 (Up)
    } else {
        bright = gamma_lut[511u - s_step];       // 255번부터 0번까지 서서히 어두워짐 (Down)
    }

    /* 하드웨어 PWM 반영 */
    uint8_t r = (uint8_t)((c->r * (uint16_t)bright) >> 8);
    uint8_t g = (uint8_t)((c->g * (uint16_t)bright) >> 8);
    uint8_t b = (uint8_t)((c->b * (uint16_t)bright) >> 8);
    rgb_set_rgb(RGB_ZONE_V_SHAPE, r, g, b);

    /* 스텝 진행 및 자동 종료 제어 */
    if (++s_step >= 512)
    {
    	if (s_fx_mode == RGB_FX_BOOT_BREATH && s_fx_include_gpio)	//일반 led (3개)
		{
			// 꺼지지 않고 100% 쨍하게 켠 상태로 고정시킵니다.
    		led_on(LED_POWER_STAT_W); // 화이트 켬 (Active Low 가정)
			led_on(LED_W_CONTROL);    // 백라이트 켬 (Active High 가정)
		}

        s_fx_mode = RGB_FX_NONE;
        s_fx_include_gpio = false; // 플래그를 꺼서 다음 rgb_tick부터는 간섭 차단!
        // 완료 후 최종 상태는 깔끔하게 소등(BLACK) 상태로 대기
        rgb_set_color(RGB_ZONE_V_SHAPE, COLOR_BLACK);


    }
}

// 이펙트 시작 전 상위 UI 레이어에서 설정하는 함수
void rgb_effect_enable_gpio_sync(bool enable)
{
    s_fx_include_gpio = enable;
}

// rgb_tick()이 가져다 쓸 수 있도록 열어주는 getter (순수 rgb.c 내부용)
bool rgb_effect_is_gpio_sync_active(void)
{
    // 이펙트가 실제로 돌고 있고, GPIO 동기화 옵션이 켜져 있을 때만 true
    return (s_fx_mode != RGB_FX_NONE) && s_fx_include_gpio;
}
