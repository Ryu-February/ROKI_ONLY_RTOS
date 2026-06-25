/*
 * uart.c
 *
 *  USART3 송신 래퍼.
 */

#include "uart.h"

extern UART_HandleTypeDef huart3;

#define UART3_TX_TIMEOUT_MS	50U

bool uart3_send(const uint8_t *buf, uint16_t len)
{
	if (buf == NULL || len == 0)
		return false;

	return (HAL_UART_Transmit(&huart3, (uint8_t *)buf, len, UART3_TX_TIMEOUT_MS) == HAL_OK);
}


void uart_printf(const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    HAL_UART_Transmit(&huart3, (uint8_t *)buf, strlen(buf), HAL_MAX_DELAY);
}
