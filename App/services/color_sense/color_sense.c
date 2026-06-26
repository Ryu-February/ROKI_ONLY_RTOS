/*
 * color_sense.c
 *
 *  Created on: 2026. 6. 10.
 *      Author: RCY
 */


#include "color_sense.h"
#include "flash.h"
#include "uart.h"


/* --------- 분류 임계값 (튜닝 파라미터) --------- */
#define REJ_RATIO_MAX        0.85f   // best/second 비율 상한 (작을수록 선명)
#define REJ_ORG_RATIO_MAX    0.92f
#define REJ_YEL_RATIO_MAX    0.92f
#define REJ_GRN_RATIO_MAX    0.90f
#define REJ_REL_MIN          0.05f   // (second-best)/second 하한
#define REJ_L_MIN_CARD       300U    // 카드모드 최소 밝기(L=R+G+B)
#define REJ_L_MIN_LINE       150U    // 라인모드 최소 밝기
#define REJ_CHROMA_REL_MIN   0.02f   // 최소 상대 채도

/* --------- RAM 기준 테이블 (부팅 시 flash에서 로드) --------- */
static reference_entry_t color_reference_tbl_left[COLOR_COUNT];
static reference_entry_t color_reference_tbl_right[COLOR_COUNT];

/* 컴파일 기본값: flash 미캘리브레이션 시 폴백.
 * 로봇별 실제 값은 캘리브레이션 후 flash에 저장되며 그쪽이 우선한다. */
static const rgb_raw_t default_raw[COLOR_COUNT] =
{
    [COLOR_GRAY]        = {  900,  900,  900 },
    [COLOR_RED]         = { 2000,  500,  500 },
    [COLOR_ORANGE]      = { 2200, 1100,  450 },
    [COLOR_YELLOW]      = { 2200, 2000,  500 },
    [COLOR_GREEN]       = {  500, 1800,  600 },
    [COLOR_BLUE]        = {  500,  700, 2000 },
    [COLOR_PURPLE]      = { 1200,  600, 1800 },
    [COLOR_LIGHT_GREEN] = { 1100, 1900,  900 },
    [COLOR_SKY_BLUE]    = {  800, 1500, 1900 },
    [COLOR_PINK]        = { 2100,  900, 1300 },
    [COLOR_BLACK]       = {  200,  200,  200 },
    [COLOR_WHITE]       = { 2200, 2200, 2200 },
};

static bool entry_is_valid(const reference_entry_t *e)
{
    // flash erase 직후(0xFFFF)는 무효로 간주
    return !(e->raw.red_raw == 0xFFFF &&
             e->raw.green_raw == 0xFFFF &&
             e->raw.blue_raw == 0xFFFF);
}

static void load_side(uint8_t side, reference_entry_t *tbl)
{
    for (int i = 0; i < COLOR_COUNT; i++)
    {
        reference_entry_t e = flash_read_color_reference(side, (uint8_t)i);
        if (!entry_is_valid(&e))
        {
            e.raw    = default_raw[i];
            e.color  = (color_t)i;
            e.offset = 0;
        }
        tbl[i] = e;
    }

	// 좌/우 구분을 위한 문자열 생성
//	    char side_char = (side == BH1749_ADDR_LEFT) ? 'L' : 'R';
//
//	    uart_printf("=== [Color Table Load] Side: %c ===\r\n", side_char);
//
//	    for (int i = 0; i < COLOR_COUNT; i++)
//	    {
//	        reference_entry_t e = flash_read_color_reference(side, (uint8_t)i);
//	        char source_str[16];
//
//	        if (!entry_is_valid(&e))
//	        {
//	            e.raw    = default_raw[i];
//	            e.color  = (color_t)i;
//	            e.offset = 0;
//	            strcpy(source_str, "DEFAULT"); // flash에 데이터가 없어 컴파일 기본값 사용
//	        }
//	        else
//	        {
//	            strcpy(source_str, "FLASH");   // 캘리브레이션된 flash 데이터 사용
//	        }
//	        tbl[i] = e;
//
//	        // 각 색상별 Index, 소스(FLASH/DEFAULT), R, G, B 값 출력
//	        uart_printf("  [%c] ColorIdx %2d (%s) -> R: %4u, G: %4u, B: %4u\r\n",
//	                    side_char, i, source_str, e.raw.red_raw, e.raw.green_raw, e.raw.blue_raw);
//	    }
//	    uart_printf("===================================\r\n");
}

void color_sense_load_references(void)
{
    load_side(BH1749_ADDR_LEFT,  color_reference_tbl_left);
    load_side(BH1749_ADDR_RIGHT, color_reference_tbl_right);
}

void color_sense_init(color_sense_t *s)
{
	color_sense_load_references();
	color_sense_reset(s);
}

void color_sense_reset(color_sense_t *s)
{
	s->count		= 0;
	s->last_stable	= COLOR_COUNT;
}

static void push_cmd(color_sense_t *s, color_mode_t cmd, color_sense_result_t *out)
{
	if (s->count < CARD_CMD_MAX)
	{
		s->cmds[s->count++] = cmd;
		out->card_inserted = true;
		out->inserted_cmd = cmd;
	}
	out->count = s->count;
}

void color_sense_update(color_sense_t *s,
                        const bh1749_color_data_t *left,
                        const bh1749_color_data_t *right,
                        color_sense_result_t *out)
{
    out->card_inserted = false;
    out->inserted_cmd  = MODE_NONE;
    out->count         = s->count;

//    (void)right;  // 현재 카드 판단은 좌측 센서 기준

    /* 1) raw -> color (호출 주기 자체가 1초이므로 streak 디바운스는 불필요) */
    color_t l = classify_color(BH1749_ADDR_LEFT, left->red, left->green, left->blue, left->ir);
    color_t r = classify_color(BH1749_ADDR_RIGHT, right->red, right->green, right->blue, right->ir);
    color_mode_t l_cmd = color_to_mode(l);
    color_mode_t r_cmd = color_to_mode(r);

    if (l_cmd == r_cmd)
    {
    	out->cur_color = l;   // PC 라이브 표시용: 카드 확정 여부와 무관하게 현재 색 보고

    	/* 2) 무채색/빈 카드 구간: 다음에 같은 색이 와도 새 카드로 인정되도록 리셋 */
		if (l_cmd == MODE_NONE)
		{
			s->last_stable = COLOR_COUNT;
			return;
		}

		/* 3) 에지: 직전 확정색과 다를 때만 1장 push (같은 카드 계속 두면 중복 방지) */
		if (l != s->last_stable)
		{
			s->last_stable = l;
			push_cmd(s, l_cmd, out);
		}
    }


}

uint8_t classify_color_side(uint8_t color_side)
{
    uint8_t addr = color_side;

    bh1749_color_data_t c = bh1749_read_rgbir(addr);
    color_t detected = classify_color(addr, c.red, c.green, c.blue, c.ir);

    return (uint8_t)detected;
}


color_t classify_color(uint8_t left_right, uint16_t r, uint16_t g, uint16_t b, uint16_t ir)
{
    (void)ir; // 원하면 glare 억제용으로 사용 가능

    // 0) 밝기/무채색 지표 계산(사전계산만, 리젝은 사후에 애매할 때만 적용)
    uint32_t L = (uint32_t)r + (uint32_t)g + (uint32_t)b;
    uint16_t maxc = r; if (g>maxc) maxc=g; if (b>maxc) maxc=b;
    uint16_t minc = r; if (g<minc) minc=g; if (b<minc) minc=b;

    const reference_entry_t* table = (left_right == BH1749_ADDR_LEFT)
                                     ? color_reference_tbl_left
                                     : color_reference_tbl_right;

    // 1) 최인접 + second-best 같이 구함
    float best = 1e30f, second = 1e30f;
    color_t best_match = COLOR_GRAY;

    for (int i = 0; i < COLOR_COUNT; i++)
    {
        float R0 = (float)table[i].raw.red_raw;
        float G0 = (float)table[i].raw.green_raw;
        float B0 = (float)table[i].raw.blue_raw;
        float denom = R0 * R0 + G0 * G0 + B0 * B0 + 1e-9f;//분모에 들어가는데 1e-9f는 10^-9임 분모가 0이 안 되게만
        float k = ((float)r * R0 + (float)g * G0 + (float)b * B0) / denom;//발기 비율차이 보정
        float dr = (float)r - k * R0;//색상이 같으면 여기서 0에 가깝게 값이 떨어짐
        float dg = (float)g - k * G0;
        float db = (float)b - k * B0;
        float dist = dr * dr + dg * dg + db * db;//유클리드 거리 측정

        if (dist < best)
        {
            second = best;
            best = dist;
            best_match = table[i].color;
        }
        else if (dist < second)
        {
            second = dist;
        }
    }

    // 2) 애매하면 거절 (라티오 + 상대마진)
    //  - Lowe’s ratio와 유사: best/second가 1에 가까우면 애매
    //  - 상대 마진: (second-best)/second (0~1)
    if (second < 1e-9f)//second가 0에 가까울 확률이 거의 없음 10^(-9)
    {
        // 보호절: 이론상 second==0은 거의 없음. 그냥 통과.
    }
    else
    {
        float ratio   = best / second;                 // 0(선명) ~ 1(애매)
        float relmag  = (second - best) / second;      // 0(애매) ~ 1(선명)

        float ratio_thresh = (best_match == COLOR_ORANGE) ? REJ_ORG_RATIO_MAX
                             : (best_match == COLOR_YELLOW) ? REJ_YEL_RATIO_MAX
							 : (best_match == COLOR_GREEN) ? REJ_GRN_RATIO_MAX
                             : REJ_RATIO_MAX;

        if (ratio > ratio_thresh || relmag < REJ_REL_MIN)
        {
            // 애매한 경우에만 밝기/채도 게이트 적용해 보수적으로 GRAY 처리
        	if (L < (REJ_L_MIN_LINE || (float)(maxc - minc) < REJ_CHROMA_REL_MIN * (float)L))
            {
                return COLOR_GRAY;
            }
        }
    }

    return best_match;
}

color_mode_t color_to_mode(color_t color)
{
    switch (color)
    {
        case COLOR_RED:         return MODE_BACKWARD;
        case COLOR_YELLOW:      return MODE_RIGHT;
        case COLOR_GREEN:       return MODE_FORWARD;
        case COLOR_BLUE:		return MODE_LEFT;
        case COLOR_PURPLE:		return MODE_REPEAT_ONCE;
        case COLOR_PINK:		return MODE_REPEAT_TWICE;
        case COLOR_SKY_BLUE:	return MODE_REPEAT_THIRD;
        case COLOR_ORANGE:		return MODE_REPEAT_FOURTH;
        default:                return MODE_NONE;
    }
}

const char* color_to_string(color_t color)
{
    static const char* color_names[] =
    {
		"GRAY",
        "RED",
        "ORANGE",
        "YELLOW",
        "GREEN",
        "BLUE",
        "PURPLE",
        "LIGHT_GREEN",
        "SKY_BLUE",
        "PINK",
        "BLACK",
        "WHITE",
    };

    if (color < 0 || color >= COLOR_COUNT)
    {
        return "UNKNOWN";
    }

    return color_names[color];
}
