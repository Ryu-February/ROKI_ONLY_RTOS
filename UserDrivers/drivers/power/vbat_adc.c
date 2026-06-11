/*
 * vbat_adc.c
 *
 *  Created on: Oct 14, 2025
 *      Author: RCY
 */

#include "vbat_adc.h"


extern ADC_HandleTypeDef hadc1;//12bit -> 0-4095



/*
 * 지금 굉장히 럭키한 상황임
 * ADC 배터리에 들어오는 저항비가 (10k | 36k)로
 * 배터리 전압이 풀 충전 상태일 때 4.1V임
 * 그때의 VBAT의 전압값: 4.1V * (36 / (10 + 36)) = 3.20869V
 * 3.2 / 4.1 == 0.78
 *  ----------------------------------------------------
 *  근데 ADC 1개당 전압 값의 공식은 VRef / ADC Resolution
 *  3.3 / 2^(12) = 0.8mV == 1ADC
 *  만약 ADC가 3000이 찍혔다고 가정해보자
 *  3000 * 0.8 = 2400 -> 현재의 VBAT으로 들어오는 전압값 : 2.4V
 *  2400 * (100 / 78) = 3076mV(현재 진짜 전압값)
 *  => 결론: 그냥 ADC 값을 그대로 전압값이라고 생각해도 됨
 *  => 다른 계산이 필요없다.
 */

uint16_t vbat_read_adc(void)
{
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
	uint16_t val = HAL_ADC_GetValue(&hadc1);
	HAL_ADC_Stop(&hadc1);
	return val; // 0-4095
}

