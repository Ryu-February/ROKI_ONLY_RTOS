/*
 * sensor_state.h
 *
 *  Created on: 2026. 6. 15.
 *      Author: RCY
 */

#ifndef SERVICES_SENSOR_STATE_SENSOR_STATE_H_
#define SERVICES_SENSOR_STATE_SENSOR_STATE_H_


#include "utils.h"
#include "color_sense.h"

typedef struct
{
	bool			ir_detected;
	bool			vbat_low;
	uint16_t		vbat_pct;

	color_t			color;		//현재 인식 중인 색 (라이브, 매 측정 갱신)
	color_mode_t	card;		//최신 확정 카드 명령
	uint8_t			card_count;	//누적 카드 개수

	uint32_t 		seq;		//갱신 카운터: PC가 변화/유실 감지용
}sensor_snapshot_t;

bool sensor_state_update_ir(bool detected);
bool sensor_state_update_bat(bool low, uint16_t vbat);
bool sensor_state_update_color(color_t color);				//현재 인식 색
bool sensor_state_update_card(color_mode_t card, uint8_t count);	//확정 카드
void sensor_state_get(sensor_snapshot_t *out);


bool sensor_state_get_ir(void);
bool sensor_state_get_vbat_low(void);
bool sensor_state_get_vbat_pct(void);
color_t      sensor_state_get_color(void);
color_mode_t sensor_state_get_card(void);
uint8_t      sensor_state_get_card_count(void);


#endif /* SERVICES_SENSOR_STATE_SENSOR_STATE_H_ */
