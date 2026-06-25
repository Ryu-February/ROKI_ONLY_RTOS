/*
 * calib_motion.c
 *
 *  Created on: 2026. 6. 25.
 *      Author: RCY
 */


#include "calib_motion.h"

#include "utils.h"
#include "msg_types.h"

#include "stepper.h"

#define CTRL_MOVE_POLL_MS	10U		// 이동 중 odometry 폴링 간격

uint32_t get_cur = 0;
uint32_t stpes = 0;

/* 현재 위치에서 steps 만큼 전진 후 정지 (blocking) */
void calib_move_forward_steps(uint32_t steps)
{
	odometry_steps_init();
	uint32_t start = get_executed_steps();


	step_drive(OP_FORWARD);
	while ((get_executed_steps() - start) < steps)
	{
		osDelay(CTRL_MOVE_POLL_MS);
		get_cur = get_executed_steps();
		stpes = steps;
	}
	step_drive(OP_STOP);
}
