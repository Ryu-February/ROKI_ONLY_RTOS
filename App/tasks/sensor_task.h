/*
 * sensor_task.h
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#ifndef TASKS_SENSOR_TASK_H_
#define TASKS_SENSOR_TASK_H_

#include "def.h"

void sensor_task(void *argument);

/* 캘리브레이션 중 평소 color 폴링 일시정지 (I2C 버스 단독 사용 보장) */
void sensor_color_pause(bool on);

#endif /* TASKS_SENSOR_TASK_H_ */
