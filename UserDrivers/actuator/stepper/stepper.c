/*
 * stepper.c  (redesigned)
 *  HARD PWM only. TIM1=left coils, TIM3=right coils (20kHz, ARR=255).
 *  TIM4 = variable-ARR step timer (1us/tick): one microstep per update IRQ.
 */
#include "stepper.h"
#include "flash.h"

/*                            Motor(15BY25-729)                         */
/*                           Motor driver(A3916)                        */

/************************************************************************/
/*                       MCU  |     NET    | DRIVER                     */
/*                      PA11 -> MOT_L_IN1 ->  IN1                       */
/*                      PA10 -> MOT_L_IN2 ->  IN2                       */
/*                      PA9  -> MOT_L_IN3 ->  IN3                       */
/*                      PA8  -> MOT_L_IN4 ->  IN4                       */
/*                                                                      */
/*                      PC9  -> MOT_R_IN1 ->  IN1                       */
/*                      PC8  -> MOT_R_IN2 ->  IN2                       */
/*                      PC7  -> MOT_R_IN3 ->  IN3                       */
/*                      PC6  -> MOT_R_IN4 ->  IN4                       */
/************************************************************************/

/*
 * A3916 Stepper Motor Operation Table (Half Step + Full Step)
 *
 * IN1 IN2 IN3 IN4 | OUT1A OUT1B OUT2A OUT2B | Function
 * --------------------------------------------------------
 *  0   0   0   0  |  Off   Off   Off   Off   | Disabled
 *  1   0   1   0  |  High  Low   High  Low   | Full Step 1 / 	½ Step 1
 *  0   0   1   0  |  Off   Off   High  Low   |             	½ Step 2
 *  0   1   1   0  |  Low   High  High  Low   | Full Step 2 /	½ Step 3
 *  0   1   0   0  |  Low   High  Off   Off   |                	½ Step 4
 *  0   1   0   1  |  Low   High  Low   High  | Full Step 3 / 	½ Step 5
 *  0   0   0   1  |  Off   Off   Low   High  |             	½ Step 6
 *  1   0   0   1  |  High  Low   Low   High  | Full Step 4 / 	½ Step 7
 *  1   0   0   0  |  High  Low   Off   Off   |             	½ Step 8
 */


// ---- External timers (provide from your BSP) ----
// TIM_PWM_CNT: ARR must be 255; we only read CNT (0..255) for software PWM compare
// TIM_TICK : free‑running tick for step period (e.g., 1us per tick)

extern TIM_HandleTypeDef htim1;   /* left  coil PWM (ARR=255, ~20kHz) */
extern TIM_HandleTypeDef htim3;   /* right coil PWM */
extern TIM_HandleTypeDef htim4;   /* step-rate timer (1us/tick, variable ARR) */

/* gear/motor set */
#define _STEP_NUM_119 0 // 15BY25-119 (gearless; CW sign differs)
#define _STEP_NUM_728 1 // 10:1
#define _STEP_NUM_729 2 // 20:1
#define _USE_STEP_NUM _STEP_NUM_729

/* ---- motors ---- */
static stepper_motor_t left =
{
    .in1_port = L_IN1_PORT, .in1_pin = L_IN1_PIN,
    .in2_port = L_IN2_PORT, .in2_pin = L_IN2_PIN,
    .in3_port = L_IN3_PORT, .in3_pin = L_IN3_PIN,
    .in4_port = L_IN4_PORT, .in4_pin = L_IN4_PIN,
    .step_idx = 0, .odometry_steps = 0,
#if (_USE_STEP_NUM == _STEP_NUM_119)
    .dir_sign = -1,
#else
    .dir_sign = +1,
#endif
};

static stepper_motor_t right =
{
    .in1_port = R_IN1_PORT, .in1_pin = R_IN1_PIN,
    .in2_port = R_IN2_PORT, .in2_pin = R_IN2_PIN,
    .in3_port = R_IN3_PORT, .in3_pin = R_IN3_PIN,
    .in4_port = R_IN4_PORT, .in4_pin = R_IN4_PIN,
    .step_idx = 0, .odometry_steps = 0,
#if (_USE_STEP_NUM == _STEP_NUM_119)
    .dir_sign = +1,
#else
    .dir_sign = -1,
#endif
};

static volatile drv_state_t g_state = DRV_COAST;
static volatile uint8_t     s_pwm_forced = 1;   /* 1: OCMODE FORCED, 0: PWM1 */

/* idx-save bookkeeping */
static volatile uint8_t s_idx_save_pending = 0;
static uint16_t s_last_saved_l = 0xFFFF, s_last_saved_r = 0xFFFF;

/* ---- LUTs ---- */

//sin table
//360도를 32스텝으로 쪼갰을 때 11.25가 나오는데 11.25도의 간격을 pwm으로 표현하면 이렇게 나옴
//여기서 말하는 360은 전류 벡터의 회전 경로가 360도라는 거고 모터는 동일하게 72도를 기준으로 잡음
#if (_USE_STEP_MODE == _STEP_MODE_MICRO)
static const uint8_t step_table[32] =     /* sin -> pwm (current-vector 360deg) */
{
    128, 152, 176, 198, 218, 234, 245, 253,
    255, 253, 245, 234, 218, 198, 176, 152,
    128, 103,  79,  57,  37,  21,  10,   2,
      0,   2,  10,  21,  37,  57,  79, 103
};
_Static_assert(sizeof(step_table) == STEP_TABLE_SIZE, "step_table size mismatch");
#elif (_USE_STEP_MODE == _STEP_MODE_FULL)
static const uint8_t step_table[4][4] =   /* {A+,A-,B+,B-} */
{
    {1,0,1,0},
	{0,1,1,0},
	{0,1,0,1},
	{1,0,0,1}
};
#else /* HALF */
static const uint8_t step_table[8][4] =
{
    {1,0,1,0},
	{0,0,1,0},
	{0,1,1,0},
	{0,1,0,0},
    {0,1,0,1},
	{0,0,0,1},
	{1,0,0,1},
	{1,0,0,0}
};
#endif

/* ---- low-level PWM (HARD) ---- */
static void hwpwm_set_ocmode_all(TIM_HandleTypeDef *htim, uint32_t mode)
{
    TIM_OC_InitTypeDef s = {0};
    s.OCMode     = mode;
    s.Pulse      = 0;
    s.OCPolarity = TIM_OCPOLARITY_HIGH;
    s.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_OC_ConfigChannel(htim, &s, TIM_CHANNEL_1);
    HAL_TIM_OC_ConfigChannel(htim, &s, TIM_CHANNEL_2);
    HAL_TIM_OC_ConfigChannel(htim, &s, TIM_CHANNEL_3);
    HAL_TIM_OC_ConfigChannel(htim, &s, TIM_CHANNEL_4);

    HAL_TIM_OC_Start(htim, TIM_CHANNEL_1);
    HAL_TIM_OC_Start(htim, TIM_CHANNEL_2);
    HAL_TIM_OC_Start(htim, TIM_CHANNEL_3);
    HAL_TIM_OC_Start(htim, TIM_CHANNEL_4);
}

static inline void hwpwm_set_left(uint8_t a_plus, uint8_t a_minus,
                                  uint8_t b_plus, uint8_t b_minus)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, a_plus);   /* PA8  A+ */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, a_minus);  /* PA9  A- */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, b_plus);   /* PA10 B+ */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, b_minus);  /* PA11 B- */
}

static inline void hwpwm_set_right(uint8_t a_plus, uint8_t a_minus,
                                   uint8_t b_plus, uint8_t b_minus)
{
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, a_plus);   /* PC6 A+ */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, a_minus);  /* PC7 A- */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, b_plus);   /* PC8 B+ */
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, b_minus);  /* PC9 B- */
}

static inline void drv_sleep(bool sleep)
{
    HAL_GPIO_WritePin(MOTOR_SLP_PORT, MOTOR_SLP_PIN,
                      sleep ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void hwpwm_use_pwm_mode(void)
{
    hwpwm_set_ocmode_all(&htim1, TIM_OCMODE_PWM1);
    hwpwm_set_ocmode_all(&htim3, TIM_OCMODE_PWM1);
    s_pwm_forced = 0;
}

static void hwpwm_brake(void)   /* both halves forced active -> short brake */
{
    hwpwm_set_ocmode_all(&htim1, TIM_OCMODE_FORCED_ACTIVE);
    hwpwm_set_ocmode_all(&htim3, TIM_OCMODE_FORCED_ACTIVE);
    s_pwm_forced = 1;
}

static void hwpwm_coast(void)   /* forced inactive -> outputs low */
{
    hwpwm_set_ocmode_all(&htim1, TIM_OCMODE_FORCED_INACTIVE);
    hwpwm_set_ocmode_all(&htim3, TIM_OCMODE_FORCED_INACTIVE);
    s_pwm_forced = 1;
}

/* ---- coil drive per step ---- */
static inline void apply_step(stepper_motor_t *m)
{
#if (_USE_STEP_MODE == _STEP_MODE_MICRO)
    uint8_t vA = step_table[m->step_idx & STEP_MASK];
    uint8_t vB = step_table[(m->step_idx + (STEP_TABLE_SIZE >> 2)) & STEP_MASK]; /* +90deg */
    if (m == &left) hwpwm_set_left (vA, (uint8_t)(255 - vA), vB, (uint8_t)(255 - vB));
    else            hwpwm_set_right(vA, (uint8_t)(255 - vA), vB, (uint8_t)(255 - vB));
#else
    const uint8_t *s = step_table[m->step_idx & STEP_MASK];
    uint8_t Ap = s[0] ? 255 : 0, Am = s[1] ? 255 : 0;
    uint8_t Bp = s[2] ? 255 : 0, Bm = s[3] ? 255 : 0;
    if (m == &left) hwpwm_set_left (Ap, Am, Bp, Bm);
    else            hwpwm_set_right(Ap, Am, Bp, Bm);
#endif
}

static inline int8_t sgn3(int8_t s)
{
    return (s > 0) ? +1 : (s < 0 ? -1 : 0);
}

/* ================= ISR ================= */
/* TIM4 update fires once per microstep (ARR = period_us-1). */
uint32_t time = 0;
uint32_t prev = 0;
uint32_t diff = 0;
void step_tick_isr(void)
{
    if (g_state != DRV_RUN) return;   /* brake/coast: hold, no advance */

    left.step_idx  = (uint16_t)((left.step_idx  + left.dir_sign)  & STEP_MASK);
    right.step_idx = (uint16_t)((right.step_idx + right.dir_sign) & STEP_MASK);
    left.odometry_steps++;
    right.odometry_steps++;

    apply_step(&left);
    apply_step(&right);
    time = TIM2->CNT;
    if (time != prev)
    {
    	time = TIM2->CNT;

    	diff = time - prev;
		prev = time;
    }

}

/* ================= control ================= */
void step_set_dir(int8_t left_sign, int8_t right_sign)
{
#if (_USE_STEP_NUM == _STEP_NUM_119)
    const int8_t L = -1, R = +1;
#else
    const int8_t L = +1, R = -1;
#endif
    left.dir_sign  = (int8_t)(sgn3(left_sign)  * L);
    right.dir_sign = (int8_t)(sgn3(right_sign) * R);
}

void step_set_period_us(uint32_t period_us)
{
    if (period_us < STEP_MIN_PERIOD_US) period_us = STEP_MIN_PERIOD_US;
    __HAL_TIM_SET_AUTORELOAD(&htim4, period_us - 1U);  /* ARPE -> next update */
}

void step_run(void)
{
    drv_sleep(false);                 /* wake driver */
    if (s_pwm_forced) hwpwm_use_pwm_mode();
    g_state = DRV_RUN;
    apply_step(&left);                /* energize current step immediately */
    apply_step(&right);
}

void step_brake(void)
{
    g_state = DRV_BRAKE;
    drv_sleep(false);                 /* brake needs driver awake to hold */
    hwpwm_brake();
}

void step_coast(void)
{
    g_state = DRV_COAST;
    hwpwm_coast();
    drv_sleep(true);                  /* release + sleep */
}

void step_stop(void)        { step_brake(); }
void step_coast_stop(void)  { step_coast(); }
bool step_is_running(void)  { return g_state == DRV_RUN; }

void step_drive(StepOperation op)
{
    switch (op)
    {
        case OP_FORWARD:    step_set_dir(+1, +1); step_run();  break;
        case OP_REVERSE:    step_set_dir(-1, -1); step_run();  break;
        case OP_TURN_LEFT:  step_set_dir(+1, -1); step_run();  break;
        case OP_TURN_RIGHT: step_set_dir(-1, +1); step_run();  break;
        case OP_STOP:       step_brake();  break;
        case OP_NONE:
        default:            step_coast();  break;
    }
}

/* ================= init ================= */
void step_init_all(void)
{
    /* coil PWM carriers (CubeMX: PSC=17, ARR=255 -> ~20kHz) */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

    /* step timer: enable ARR preload, set default rate.
       (TIM4 base IRQ is started in bsp_isr_tim_start) */
    htim4.Instance->CR1 |= TIM_CR1_ARPE;
    step_set_period_us(STEP_DEFAULT_PERIOD_US);

    left.step_idx = right.step_idx = 0;
    left.odometry_steps = right.odometry_steps = 0;

    step_coast();   /* safe boot state */
}

/* ================= telemetry ================= */
uint32_t get_executed_steps(void)
{
    return (left.odometry_steps + right.odometry_steps) / 2u;
}

void show_executed_steps(void)
{
    uart_printf("L : %lu || R: %lu\r\n",
                (unsigned long)left.odometry_steps,
                (unsigned long)right.odometry_steps);
}

void odometry_steps_init(void)
{
    left.odometry_steps = right.odometry_steps = 0;
}

/* ================= step index persistence ================= */
void step_idx_init(void) { left.step_idx = right.step_idx = 0; }

void step_idx_mark_save_needed(void) { s_idx_save_pending = 1; }

void step_idx_flush_service(void)
{
    if (!s_idx_save_pending)      return;
    if (g_state == DRV_RUN)       return;   /* save only when stopped */

    uint16_t cur_l = left.step_idx;
    uint16_t cur_r = right.step_idx;
    if (cur_l == s_last_saved_l && cur_r == s_last_saved_r)
    {
        s_idx_save_pending = 0;
        return;
    }

    flash_erase_step_index();
    flash_write_step_index(cur_l, cur_r);
    s_last_saved_l = cur_l;
    s_last_saved_r = cur_r;
    s_idx_save_pending = 0;
    step_idx_show();
}

void step_idx_save(void)
{
    flash_erase_step_index();
    flash_write_step_index(left.step_idx, right.step_idx);
}

void step_idx_show(void)
{
    uart_printf("[STOP] step_idx L=%u R=%u\r\n",
                (unsigned)left.step_idx, (unsigned)right.step_idx);
}
