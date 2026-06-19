/*
 * bsp_isr.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */



#include "bsp_isr.h"

#include "utils.h"

#include "rgb.h"
#include "rgb_effect.h"
#include "buzzer.h"
#include "stepper.h"

#include "msg_types.h"

#define UART_RX_DMA_BUFSZ	128


extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim7;

extern UART_HandleTypeDef huart3;


uint8_t rx_dma_buf[UART_RX_DMA_BUFSZ];

void bsp_isr_tim_start(void)
{
	HAL_TIM_Base_Start_IT(&htim2); // stepper period timer
	HAL_TIM_Base_Start_IT(&htim4); //stepper motor timer

	HAL_TIM_Base_Start_IT(&htim6); // 1ms timer
	HAL_TIM_Base_Start_IT(&htim7); // rgb led timer

	HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_dma_buf, UART_RX_DMA_BUFSZ);
}


void bsp_isr_tim4_callback(void) /*300us timer - stepper motor timer*/
{
	step_tick_isr();
}

void bsp_isr_tim6_callback(void)
{
	buzzer_update_1ms();
}

void bsp_isr_tim7_callback(void) /*50us timer - rgb led timer*/
{
	rgb_tick();
	rgb_effect_tick();
}

static volatile uint16_t rx_last_pos = 0;

void bsp_usart3_gpdma_ch1_callback(uint16_t size)
{
	if (size == rx_last_pos)
	{
		return;
	}

	if (size > rx_last_pos)
	{
		for (uint16_t i = rx_last_pos; i < size; i++)
		{
			osMessageQueuePut(rx_queue, &rx_dma_buf[i], 0, 0);
		}
	}
	else
	{
		for (uint16_t i = rx_last_pos; i < UART_RX_DMA_BUFSZ; i++)
		{
			osMessageQueuePut(rx_queue, &rx_dma_buf[i], 0, 0);
		}
		for (uint16_t i = 0; i < size; i++)
		{
			osMessageQueuePut(rx_queue, &rx_dma_buf[i], 0, 0);
		}
	}
	rx_last_pos = size;
}


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
	if (huart->Instance == USART3)
		bsp_usart3_gpdma_ch1_callback(size);
}

