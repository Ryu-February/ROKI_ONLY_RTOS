/*
 * protocol.c
 *
 *  ROAND RealTime Data Protocol - frame parser / encoder (HW 독립)
 */

#include "protocol.h"

void protocol_parser_reset(proto_parser_t *p)
{
	p->state = PROTO_WAIT_STX1;
	p->sum   = 0;
	p->idx   = 0;
}

bool protocol_feed(proto_parser_t *p, uint8_t b, proto_frame_t *out)
{
	switch (p->state)
	{
		case PROTO_WAIT_STX1:
			if (b == PROTO_STX1)
			{
				p->sum   = b;
				p->state = PROTO_WAIT_STX2;
			}
			break;

		case PROTO_WAIT_STX2:
			if (b == PROTO_STX2)
			{
				p->sum  += b;
				p->state = PROTO_GET_CMD;
			}
			else
			{
				/* 헤더 깨짐: 이 바이트가 새 STX1 일 수도 있으니 재평가 */
				protocol_parser_reset(p);
				if (b == PROTO_STX1)
				{
					p->sum   = b;
					p->state = PROTO_WAIT_STX2;
				}
			}
			break;

		case PROTO_GET_CMD:
			p->frame.cmd = b;
			p->sum      += b;
			p->state     = PROTO_GET_LEN;
			break;

		case PROTO_GET_LEN:
			p->frame.len = b;
			p->sum      += b;
			p->idx       = 0;
			p->state     = (b == 0) ? PROTO_GET_CHK : PROTO_GET_DATA;
			break;

		case PROTO_GET_DATA:
			p->frame.data[p->idx++] = b;
			p->sum                 += b;
			if (p->idx >= p->frame.len)
				p->state = PROTO_GET_CHK;
			break;

		case PROTO_GET_CHK:
			p->sum += b;
			{
				bool ok = ((uint8_t)(p->sum) == 0U);	/* 전체 합 0 이어야 유효 */
				if (ok && out)
				{
					out->cmd = p->frame.cmd;
					out->len = p->frame.len;
					memcpy(out->data, p->frame.data, p->frame.len);
				}
				protocol_parser_reset(p);
				return ok;
			}

		default:
			protocol_parser_reset(p);
			break;
	}

	return false;
}

uint16_t protocol_encode(uint8_t cmd, const uint8_t *data, uint8_t len,
						 uint8_t *out, uint16_t out_sz)
{
	uint16_t total = (uint16_t)(4U + len + 1U);
	if (out_sz < total)
		return 0;

	uint8_t sum = 0;

	out[0] = PROTO_STX1;	sum += PROTO_STX1;
	out[1] = PROTO_STX2;	sum += PROTO_STX2;
	out[2] = cmd;			sum += cmd;
	out[3] = len;			sum += len;

	for (uint8_t i = 0; i < len; i++)
	{
		out[4 + i] = data[i];
		sum       += data[i];
	}

	out[4 + len] = (uint8_t)(0U - sum);	/* 2의 보수: 전체 합이 0 */

	return total;
}
