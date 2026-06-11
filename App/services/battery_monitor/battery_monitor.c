/*
 * battery_monitor.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */



#include "battery_monitor.h"







/* 유효 ADC 구간 (대략): idle 3200~4100, running 2900~4100 */
#define VBAT_ADC_FULL			4100U
#define VBAT_ADC_EMPTY_IDLE		3200U
#define VBAT_ADC_EMPTY_RUN		2900U

/* 히스테리시스 임계 (%) */
#define VBAT_LOW_ENTER_IDLE		50U
#define VBAT_LOW_ENTER_RUN		42U
#define VBAT_LOW_CLEAR_IDLE		60U

static uint32_t pct_map(uint16_t v, uint16_t empty, uint16_t full)
{
	if (v <= empty)
		return 0U;
	if (v >= full)
		return 100U;
	return ((uint32_t)(v - empty) * 100U) / (uint32_t)(full - empty);
}


void battery_monitor_update(battery_state_t *st, uint16_t adc, bool running,
							battery_result_t *out)
{
	/* 1. 모터가 안 돌 때만 rest 평활 (러닝 중 peak 튐 방지) */
	if (!running)
	{
		if (!st->rest_valid)
		{
			st->rest_adc = adc;
			st->rest_valid = 1U;
		}
		else
		{
			st->rest_adc = (uint16_t)(((uint32_t)st->rest_adc * 3U + adc) / 4U);
		}
	}

	/* 2. 상태별 구간 환산 */
	uint32_t pct_idle = pct_map(adc, VBAT_ADC_EMPTY_IDLE, VBAT_ADC_FULL);
	uint32_t pct_run  = pct_map(adc, VBAT_ADC_EMPTY_RUN, VBAT_ADC_FULL);

	/* 3. 히스테리시스: 진입/해제 기준 분리 */
	if (st->low)
	{
		/* 러닝 중에는 해제 금지, 아이들에서만 해제 */
		if (!running && pct_idle >= VBAT_LOW_CLEAR_IDLE)
			st->low = 0U;
	}
	else
	{
		uint32_t enter_th  = running ? VBAT_LOW_ENTER_RUN : VBAT_LOW_ENTER_IDLE;
		uint32_t pct_enter = running ? pct_run : pct_idle;
		if (pct_enter <= enter_th)
			st->low = 1U;
	}

	/* 4. 결과 출력 (표시용 pct는 idle 기준) */
	out->low = (st->low != 0U);
	out->pct = (uint8_t)pct_idle;
}
