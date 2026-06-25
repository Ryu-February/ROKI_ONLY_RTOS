/*
 * stepper.h
 *
 *  Created on: Sep 10, 2025
 *      Author: RCY
 *  	15BY25-729 + A3916, STM32U375
 */
#ifndef ACTUATOR_STEPPER_STEPPER_H_
#define ACTUATOR_STEPPER_STEPPER_H_

#include "def.h"

/* ================= STM32U375RGT6 Pin Map =================
 * 15BY25-729 + A3916
 *  MCU  |   NET      | DRIVER
 * PA11 -> MOT_L_IN1  -> IN1
 * PA10 -> MOT_L_IN2  -> IN2
 * PA9  -> MOT_L_IN3  -> IN3
 * PA8  -> MOT_L_IN4  -> IN4
 *
 * PC9  -> MOT_R_IN1  -> IN1
 * PC8  -> MOT_R_IN2  -> IN2
 * PC7  -> MOT_R_IN3  -> IN3
 * PC6  -> MOT_R_IN4  -> IN4
 * ======================================================== */

#define L_IN1_PORT GPIOA
#define L_IN1_PIN  GPIO_PIN_11
#define L_IN2_PORT GPIOA
#define L_IN2_PIN  GPIO_PIN_10
#define L_IN3_PORT GPIOA
#define L_IN3_PIN  GPIO_PIN_9
#define L_IN4_PORT GPIOA
#define L_IN4_PIN  GPIO_PIN_8

#define R_IN1_PORT GPIOC
#define R_IN1_PIN  GPIO_PIN_9
#define R_IN2_PORT GPIOC
#define R_IN2_PIN  GPIO_PIN_8
#define R_IN3_PORT GPIOC
#define R_IN3_PIN  GPIO_PIN_7
#define R_IN4_PORT GPIOC
#define R_IN4_PIN  GPIO_PIN_6

/* Motor driver sleep (nSLEEP) */
#define MOTOR_SLP_PORT GPIOA
#define MOTOR_SLP_PIN  GPIO_PIN_12

/* Step rate. TIM4: PSC=95 -> 1us/tick, ARR = period_us - 1. */
#define STEP_DEFAULT_PERIOD_US 500U   /* legacy base speed */
#define STEP_MIN_PERIOD_US     30U   /* cap top speed (anti step-loss) */

#define SAFE_MAX_RPM 1200

/* ---- Step mode ---- */
#define _STEP_MODE_FULL  0
#define _STEP_MODE_HALF  1
#define _STEP_MODE_MICRO 2
#ifndef _USE_STEP_MODE
#define _USE_STEP_MODE _STEP_MODE_MICRO
#endif

#if   (_USE_STEP_MODE == _STEP_MODE_HALF)
#define STEP_MASK 0x07
#define STEP_PER_REV 40
#define STEP_TABLE_SIZE 8
#elif (_USE_STEP_MODE == _STEP_MODE_FULL)
#define STEP_MASK 0x03
#define STEP_PER_REV 20
#define STEP_TABLE_SIZE 4
#elif (_USE_STEP_MODE == _STEP_MODE_MICRO)
#define STEP_MASK 0x1F
#define STEP_PER_REV 80
#define STEP_TABLE_SIZE 32
#else
#error "_USE_STEP_MODE invalid"
#endif

/* ---- Types ---- */
typedef struct
{
    GPIO_TypeDef *in1_port;  uint16_t in1_pin;
    GPIO_TypeDef *in2_port;  uint16_t in2_pin;
    GPIO_TypeDef *in3_port;  uint16_t in3_pin;
    GPIO_TypeDef *in4_port;  uint16_t in4_pin;

    volatile uint16_t step_idx;        /* 0..STEP_MASK */
    volatile uint32_t odometry_steps;  /* telemetry (32b read atomic on M4) */
    int8_t            dir_sign;        /* +1 / -1 */
} stepper_motor_t;

typedef enum
{
    DRV_COAST = 0,   /* coils off, driver asleep   */
    DRV_RUN,         /* stepping, driver awake     */
    DRV_BRAKE        /* coils energized, awake     */
} drv_state_t;

typedef enum
{
    OP_NONE = 0,
    OP_FORWARD,
    OP_REVERSE,
    OP_TURN_LEFT,
    OP_TURN_RIGHT,
    OP_STOP
} StepOperation;

/* ---- API ---- */
void step_init_all(void);
void step_tick_isr(void);                 /* call from TIM4 update ISR */

void step_set_dir(int8_t left_sign, int8_t right_sign);  /* direction only */
void step_set_period_us(uint32_t period_us);             /* shared step rate */

void step_run(void);
void step_brake(void);
void step_coast(void);
void step_stop(void);        /* = brake */
void step_coast_stop(void);  /* = coast */
bool step_is_running(void);

void step_drive(StepOperation op);

/* steps 만큼 전진 후 ISR에서 자동 정지. 완료 시 step_on_move_done() 호출 */
void step_move_steps(uint32_t steps);
/* 목표 스텝 도달 완료 콜백 (ISR 컨텍스트). 필요한 태스크에서 override */
void step_on_move_done(void);

/* telemetry */
uint32_t get_executed_steps(void);
void show_executed_steps(void);
void odometry_steps_init(void);

/* step index persistence */
void step_idx_init(void);
void step_idx_save(void);
void step_idx_show(void);
void step_idx_mark_save_needed(void);
void step_idx_flush_service(void);

#endif /* ACTUATOR_STEPPER_STEPPER_H_ */
