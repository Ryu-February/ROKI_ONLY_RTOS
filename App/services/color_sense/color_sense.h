/*
 * color_sense.h
 *
 *  Created on: 2026. 6. 10.
 *      Author: RCY
 */

#ifndef SERVICES_COLOR_SENSE_COLOR_SENSE_H_
#define SERVICES_COLOR_SENSE_COLOR_SENSE_H_


#include "def.h"

#include "color.h"

#include "card_cmd.h"

#define CARD_CMD_MAX		MAX_INSERTED_COMMANDS

typedef struct
{
	color_mode_t 	cmds[CARD_CMD_MAX];	// 누적된 카드 명령
	uint8_t			count;				// 쌓인 개수

	color_t 		last_stable;		// 직전 확정 색 (에지 검출용)
}color_sense_t;


typedef struct
{
	color_t			cur_color;			// 이번 호출에 인식된 현재 색 (라이브, MODE_NONE 무관)
	bool			card_inserted;		// 이번 호출에 카드 1장 확정됐나
	color_mode_t	inserted_cmd;		// 확정된 명령
	uint8_t			count;				// 현재 누적 개수
}color_sense_result_t;

void color_sense_init(color_sense_t *s);
void color_sense_reset(color_sense_t *s);

void color_sense_update(color_sense_t *s,
						const bh1749_color_data_t *left,
						const bh1749_color_data_t *right,
						color_sense_result_t *out);

/* 부팅 시 1회: flash 기준 테이블을 RAM으로 로드 (없으면 기본값) */
void         color_sense_load_references(void);

color_t      classify_color(uint8_t left_right, uint16_t r, uint16_t g, uint16_t b, uint16_t ir);
uint8_t      classify_color_side(uint8_t color_side);
color_mode_t color_to_mode(color_t color);
const char* color_to_string(color_t color);

#endif /* SERVICES_COLOR_SENSE_COLOR_SENSE_H_ */
