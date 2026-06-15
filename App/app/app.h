/*
 * app.h
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#ifndef APP_APP_H_
#define APP_APP_H_


#include "utils.h"
#include "msg_types.h"

#include "ui_task.h"
#include "input_task.h"
#include "sensor_task.h"
#include "control_task.h"

typedef struct
{
	osThreadFunc_t 	func;
	const char 		*name;
	uint32_t 		stack_size;
	osPriority_t 	priority;
}task_init_t;


void app_init(void);


#endif /* APP_APP_H_ */
