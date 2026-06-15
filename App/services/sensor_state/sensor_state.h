/*
 * sensor_state.h
 *
 *  Created on: 2026. 6. 15.
 *      Author: RCY
 */

#ifndef SERVICES_SENSOR_STATE_SENSOR_STATE_H_
#define SERVICES_SENSOR_STATE_SENSOR_STATE_H_


#include "utils.h"

typedef struct
{
	bool		ir_detected;
	bool		vbat_low;
	uint16_t	vbat_pct;
	uint32_t 	seq;		//갱신 카운터: PC가 변화/유실 감지용
}sensor_snapshot_t;

bool sensor_state_update_ir(bool detected);
bool sensor_state_update_bat(bool low, uint16_t vbat);
void sensor_state_get(sensor_snapshot_t *out);


bool sensor_state_get_ir(void);
bool sensor_state_get_vbat_low(void);
bool sensor_state_get_vbat_pct(void);


#endif /* SERVICES_SENSOR_STATE_SENSOR_STATE_H_ */
