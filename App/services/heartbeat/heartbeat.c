/*
 * heartbeat.c
 *
 *  Created on: 2026. 6. 17.
 *      Author: RCY
 */

#include "heartbeat.h"
#include "supervisor_task.h"

#include "utils.h"



static uint32_t s_div = 0;

extern IWDG_HandleTypeDef hiwdg;


void heartbeat_init(void)
{
	s_div = 0;
}


void heartbeat_tick(void)
{
	HAL_IWDG_Refresh(&hiwdg);		//IWDG_Refresh로 갱신 못할 시 자동 종료 프로그램
}
