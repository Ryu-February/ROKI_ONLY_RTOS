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

void ui_feedback_init(void);

void ui_feedback_btn_press_start(btn_id_t btn);

void ui_feedback_btn_press_timeout(void);
void ui_feedback_indicate_battery(bool low);

void ui_feedback_on_obstacle(uint16_t detected);

void ui_feedback_indicate_card(color_mode_t cmd);

#endif /* SERVICES_UI_FEEDBACK_UI_FEEDBACK_H_ */
