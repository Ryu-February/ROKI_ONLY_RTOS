/*
 * uart.h
 *
 *  USART3 송신 래퍼 (PC 프로토콜 TX). HW 의존부는 여기로 격리.
 */

#ifndef BSP_UART_UART_H_
#define BSP_UART_UART_H_

#include "def.h"

/* 블로킹 송신. 반환 true = 성공 */
bool uart3_send(const uint8_t *buf, uint16_t len);

#endif /* BSP_UART_UART_H_ */
