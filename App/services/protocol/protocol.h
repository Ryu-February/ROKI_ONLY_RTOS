/*
 * protocol.h
 *
 *  ROAND RealTime Data Protocol (frame layer)
 *
 *  Frame:  [0xA5][0x5A][CMD][LEN=N][Data 0..N-1][Chk]
 *  Chk  =  ~(0xA5 + 0x5A + CMD + LEN + Data[0..N-1]) + 1
 *          → (전체 패킷 바이트 합) & 0xFF == 0
 *
 *  HW 독립: 파싱/인코딩만. 송수신은 comms_task / bsp uart 담당.
 */

#ifndef SERVICES_PROTOCOL_PROTOCOL_H_
#define SERVICES_PROTOCOL_PROTOCOL_H_

#include "def.h"

#define PROTO_STX1		0xA5U
#define PROTO_STX2		0x5AU
#define PROTO_MAX_DATA	255U					/* LEN 이 1byte */
#define PROTO_FRAME_MAX	(4U + PROTO_MAX_DATA + 1U)	/* STX1 STX2 CMD LEN data Chk */

typedef enum
{
	PROTO_WAIT_STX1 = 0,
	PROTO_WAIT_STX2,
	PROTO_GET_CMD,
	PROTO_GET_LEN,
	PROTO_GET_DATA,
	PROTO_GET_CHK
}proto_state_t;

typedef struct
{
	uint8_t cmd;
	uint8_t len;
	uint8_t data[PROTO_MAX_DATA];
}proto_frame_t;

typedef struct
{
	proto_state_t state;
	uint8_t       sum;		/* STX1 부터 누적(1byte) */
	uint8_t       idx;		/* data 수신 인덱스 */
	proto_frame_t frame;
}proto_parser_t;

/* 파서 초기화 / 리셋 */
void protocol_parser_reset(proto_parser_t *p);

/* 바이트 1개 투입. 유효한 완성 프레임이면 true 반환 + out 채움.
 * 체크섬 불일치 시 내부 리셋하고 false. */
bool protocol_feed(proto_parser_t *p, uint8_t b, proto_frame_t *out);

/* 응답/요청 프레임 인코딩. 반환값 = 총 바이트 수(0 = 버퍼 부족).
 * out 은 최소 PROTO_FRAME_MAX 권장. */
uint16_t protocol_encode(uint8_t cmd, const uint8_t *data, uint8_t len,
						 uint8_t *out, uint16_t out_sz);

#endif /* SERVICES_PROTOCOL_PROTOCOL_H_ */
