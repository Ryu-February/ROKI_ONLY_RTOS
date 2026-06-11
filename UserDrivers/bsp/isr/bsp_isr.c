/*
 * bsp_isr.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */



#include "bsp_isr.h"

#include "utils.h"

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim7;


void bsp_isr_tim_start(void)
{
	HAL_TIM_Base_Start_IT(&htim2); // stepper period timer
	HAL_TIM_Base_Start_IT(&htim4); //stepper motor timer

	HAL_TIM_Base_Start_IT(&htim6); // 1ms timer
	HAL_TIM_Base_Start_IT(&htim7); // rgb led timer
}


void bsp_isr_tim4_callback(void) /*30us timer - stepper motor timer*/
{

}

void bsp_isr_tim6_callback(void)
{

}

void bsp_isr_tim7_callback(void) /*50us timer - rgb led timer*/
{

}
