/*
 * ir_monitor.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */


#include "ir_monitor.h"
#include "ir.h"

#include "msg_types.h"



void ir_monitor_update(uint16_t adc, ir_result_t *out)
{
	if (adc > 700)
		out->ir_detected = true;
	else
		out->ir_detected = false;
}
