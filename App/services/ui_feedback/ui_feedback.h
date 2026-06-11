/*
 * ui_feedback.h
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#ifndef SERVICES_UI_FEEDBACK_UI_FEEDBACK_H_
#define SERVICES_UI_FEEDBACK_UI_FEEDBACK_H_


#include "btn.h"

void ui_feedback_init(void);

void ui_feedback_btn_press_start(btn_id_t btn);

void ui_feedback_btn_press_timeout(void);

#endif /* SERVICES_UI_FEEDBACK_UI_FEEDBACK_H_ */
