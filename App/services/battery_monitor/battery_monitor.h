/*
 * battery_monitor.h
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#ifndef SERVICES_BATTERY_MONITOR_BATTERY_MONITOR_H_
#define SERVICES_BATTERY_MONITOR_BATTERY_MONITOR_H_


#include "def.h"

typedef struct
{
	uint16_t 	rest_adc;
	uint8_t		rest_valid;
	uint8_t 	low;
}battery_state_t;

typedef struct
{
	bool		low;
	uint8_t		pct;
}battery_result_t;


void battery_monitor_update(battery_state_t *st, uint16_t adc, bool running,
							battery_result_t *res);



#endif /* SERVICES_BATTERY_MONITOR_BATTERY_MONITOR_H_ */
