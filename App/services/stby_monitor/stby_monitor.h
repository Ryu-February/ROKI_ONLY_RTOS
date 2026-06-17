/*
 * stby_monitor.h
 *
 *  Created on: 2026. 6. 16.
 *      Author: RCY
 */

#ifndef SERVICES_STBY_MONITOR_STBY_MONITOR_H_
#define SERVICES_STBY_MONITOR_STBY_MONITOR_H_


#include "def.h"

#define LP_STBY_DEBOUNCE_MS  		30
#define LP_STBY_HOLD_MS      		100		// 500ms(100 & 5ms)


void stby_monitor_update(bool pressed);
void enter_standby_safe(void);

#endif /* SERVICES_STBY_MONITOR_STBY_MONITOR_H_ */
