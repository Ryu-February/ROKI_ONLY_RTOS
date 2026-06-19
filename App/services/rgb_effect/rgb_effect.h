/*
 * rgb_effect.h
 *
 *  Created on: 2026. 6. 19.
 *      Author: RCY
 */

#ifndef RGB_EFFECT_RGB_EFFECT_H_
#define RGB_EFFECT_RGB_EFFECT_H_

#include "rgb.h"

typedef enum {
    RGB_FX_NONE = 0,
    RGB_FX_BOOT_BREATH,      // Black -> White 서서히 켜짐
    RGB_FX_SHUTDOWN_FADE,    // White -> Black 서서히 꺼짐
    RGB_FX_CARD_FLASH,       // 카드 인식 시 순간 번쩍
} rgb_fx_mode_t;

void rgb_effect_init(void);
void rgb_effect_start(rgb_fx_mode_t mode, color_t target_color);
void rgb_effect_tick(void); // 60us TIM ISR에서 호출

void rgb_effect_enable_gpio_sync(bool enable);
bool rgb_effect_is_gpio_sync_active(void);

#endif /* RGB_EFFECT_RGB_EFFECT_H_ */
