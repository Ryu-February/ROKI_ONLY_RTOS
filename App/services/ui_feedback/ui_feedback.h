/*
 * ui_feedback.h
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#ifndef SERVICES_UI_FEEDBACK_UI_FEEDBACK_H_
#define SERVICES_UI_FEEDBACK_UI_FEEDBACK_H_


#include "btn.h"
#include "card_cmd.h"
#include "rgb.h"

void ui_feedback_init(void);

void ui_feedback_btn_press_start(btn_id_t btn);

void ui_feedback_btn_press_timeout(void);
void ui_feedback_indicate_battery(bool low);

void ui_feedback_on_obstacle(uint16_t detected);

void ui_feedback_indicate_card(color_mode_t cmd);
void ui_feedback_disable(void);
void ui_feedback_power_on(void);

void ui_feedback_boot_on(void);
void ui_feedback_calib_on(void);
void ui_feedback_calib_start(void);
void ui_feedback_calib_show_color(color_t color);
void ui_feedback_calib_done(void);
#endif /* SERVICES_UI_FEEDBACK_UI_FEEDBACK_H_ */
