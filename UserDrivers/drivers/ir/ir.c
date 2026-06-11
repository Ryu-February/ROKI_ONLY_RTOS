/*
 * ir.c
 *
 *  Created on: 2026. 3. 16.
 *      Author: RCY
 */



#include "ir.h"
#include "color.h"


extern ADC_HandleTypeDef hadc1; // from main.c

uint16_t ir_read_adc(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    // 1) PA1 (ADC1_IN4)로 채널 설정
    sConfig.Channel      = ADC_CHANNEL_4;           // PA1
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_246CYCLES_5;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset       = 0;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
        return 0;

    // 2) 변환 시작 및 값 읽기
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
        return 0;
    if (HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return 0;
    }
    uint16_t ir_val = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // 3) 배터리 채널(PA2, ADC_CHANNEL_5)로 복구
    sConfig.Channel = ADC_CHANNEL_5;                // PA2
    (void)HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    return ir_val;                                  // 0~4095
}
