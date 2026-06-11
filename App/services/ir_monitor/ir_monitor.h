/*
 * ir_monitor.h
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#ifndef SERVICES_IR_MONITOR_IR_MONITOR_H_
#define SERVICES_IR_MONITOR_IR_MONITOR_H_

#include "def.h"

typedef struct
{
	bool ir_detected;
}ir_result_t;


void ir_monitor_update(uint16_t adc, ir_result_t *out);

#endif /* SERVICES_IR_MONITOR_IR_MONITOR_H_ */
