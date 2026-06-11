/*
 * board.h
 *
 *  Created on: 2026. 6. 8.
 *      Author: RCY
 */

#ifndef BSP_BOARD_H
#define BSP_BOARD_H
#include "main.h"   // CubeMX GPIO/handle 정의

/* ===== 주변장치 핸들 (CubeMX 생성) ===== */
// htim2  : (스테퍼 스텝 타이밍 추정 — 확인 필요)
// htim4  : (스테퍼 스텝 타이밍 추정 — 확인 필요)
// htim6  : 1ms 시스템 틱 (timer6_ms / tim6_get_ms)
// htim16 : 부저 PWM
// huart3 : PC 통신 (DMA RX, ReceiveToIdle)
// hi2c1  : (단, 실제는 bsp/i2c.c 베어메탈 I2C1 사용)
// hiwdg  : 워치독
// hadc1  : VBAT 측정 (12-bit)
// GPDMA1 : UART RX DMA

/* ===== I2C1 (베어메탈, PB8=SCL / PB9=SDA, AF4) ===== */
#define BOARD_I2C_SCL_PIN     GPIO_PIN_8   // GPIOB
#define BOARD_I2C_SDA_PIN     GPIO_PIN_9   // GPIOB
#define BOARD_I2C_TIMINGR     0x00303D5BUL // 100kHz @ HSI16

/* ===== I2C 디바이스 주소 (7-bit) ===== */
#define BOARD_IMU_ADDR        0x68  // ICM-42670 (ALT 0x69)
#define BOARD_COLOR_L_ADDR    0x38  // BH1749 Left
#define BOARD_COLOR_R_ADDR    0x39  // BH1749 Right

/* ===== 디버그/상태 LED ===== */
#define LED_PWR_W_PORT  GPIOC
#define LED_PWR_W_PIN   GPIO_PIN_4   // low active
#define LED_PWR_O_PORT  GPIOC
#define LED_PWR_O_PIN   GPIO_PIN_5   // low active
#define LED_CTRL_PORT   GPIOA
#define LED_CTRL_PIN    GPIO_PIN_7   // high active (npn-tr base)

/* ===== RGB (전부 GPIOC, active-low) ===== */
#define RGB_PORT        GPIOC
#define RGB_V_B_PIN     GPIO_PIN_0   // V-shape
#define RGB_V_R_PIN     GPIO_PIN_1
#define RGB_V_G_PIN     GPIO_PIN_2
#define RGB_E_B_PIN     GPIO_PIN_10  // Eyes
#define RGB_E_R_PIN     GPIO_PIN_11
#define RGB_E_G_PIN     GPIO_PIN_12

/* ===== 스테퍼 모터 ===== */
#define MOT_L_IN1_PORT  GPIOA
#define MOT_L_IN1_PIN   GPIO_PIN_11
#define MOT_L_IN2_PIN   GPIO_PIN_10  // GPIOA
#define MOT_L_IN3_PIN   GPIO_PIN_9   // GPIOA
#define MOT_L_IN4_PIN   GPIO_PIN_8   // GPIOA
#define MOT_R_IN1_PORT  GPIOC
#define MOT_R_IN1_PIN   GPIO_PIN_9
#define MOT_R_IN2_PIN   GPIO_PIN_8   // GPIOC
#define MOT_R_IN3_PIN   GPIO_PIN_7   // GPIOC
#define MOT_R_IN4_PIN   GPIO_PIN_6   // GPIOC
#define MOT_SLEEP_PORT  GPIOA
#define MOT_SLEEP_PIN   GPIO_PIN_12  // A3919 sleep (전원 off 시 RESET)
// PA3 : A3919 sleepn (코드상 "있으나 마나" 주석 — 정리 대상)

/* ===== 버튼 (전부 GPIOB, active-low) ===== */
#define BTN_PORT        GPIOB
#define BTN_EXECUTE_PIN  GPIO_PIN_0
#define BTN_RESUME_PIN   GPIO_PIN_1
#define BTN_DELETE_PIN   GPIO_PIN_2
#define BTN_FORWARD_PIN  GPIO_PIN_12
#define BTN_BACKWARD_PIN GPIO_PIN_13
#define BTN_LEFT_PIN     GPIO_PIN_14
#define BTN_RIGHT_PIN    GPIO_PIN_15

/* ===== 모드 스위치 (전부 GPIOB, active-low) ===== */
#define MODE_SW_PORT       GPIOB
#define MODE_SW_BUTTON_PIN GPIO_PIN_3
#define MODE_SW_CARD_PIN   GPIO_PIN_4
#define MODE_SW_LINE_PIN   GPIO_PIN_5

/* ===== 전원 버튼 / 웨이크 (active-low) ===== */
#define PWR_BTN_PORT    GPIOA
#define PWR_BTN_PIN     GPIO_PIN_0   // standby wake

/* ===== 부저 ===== */
#define BUZZER_TIM_HANDLE   htim16
#define BUZZER_TIM_CH       TIM_CHANNEL_1   // PA6
#define BUZZER_TIM_CLK_HZ   1000000UL       // PSC=95

/* ===== VBAT ADC ===== */
#define VBAT_ADC_HANDLE     hadc1
#define VBAT_ADC_FULL       4100
#define VBAT_ADC_EMPTY_IDLE 3200
#define VBAT_ADC_EMPTY_RUN  2900

#endif
