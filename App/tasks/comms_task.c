/*
 * comms_task.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */
#include "comms_task.h"

#include "utils.h"
#include "msg_types.h"
#include "protocol.h"
#include "uart.h"


static proto_parser_t s_parser;

/* 응답 송신: cmd + data 로 프레임 인코딩 후 USART3 전송 */
static void comms_respond(uint8_t cmd, const uint8_t *data, uint8_t len)
{
	uint8_t  buf[PROTO_FRAME_MAX];
	uint16_t n = protocol_encode(cmd, data, len, buf, sizeof(buf));
	if (n > 0)
		uart3_send(buf, n);
}

/* 수신된 완성 프레임 처리.
 * TODO: 로봇별 CMD 매핑은 ROAND 프로토콜 시트에 맞춰 여기서 채움.
 *       (모터/센서/사운드/모드 등 → ctrl_queue, ui_queue 로 디스패치) */
static void comms_dispatch(const proto_frame_t *f)
{
	switch (f->cmd)
	{
		/* 예: CMD - GET VOLTAGE [0x7C] → 배터리값 응답 */
		/* case 0x7C: { ... comms_respond(0x7C, payload, n); } break; */

		/* 예: 모터 명령 → ctrl_queue 발행 */
		/* case 0x..: { ctrl_msg_t m = {...}; osMessageQueuePut(ctrl_queue, &m, 0, 0); } break; */

		default:
			(void)f;
			break;
	}
}

void comms_task(void *argument)
{
	(void)argument;

	protocol_parser_reset(&s_parser);

	static proto_frame_t frame;	/* 큰 구조체(~257B) → 스택 절약 위해 static */
	uint8_t              b;

	for (;;)
	{
		/* bsp_isr 가 rx_queue 로 바이트  단위 적재 (DMA RX-to-idle) */
		if (osMessageQueueGet(rx_queue, &b, NULL, osWaitForever) != osOK)
			continue;

		if (protocol_feed(&s_parser, b, &frame))
			comms_dispatch(&frame);
	}
}
