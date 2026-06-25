/*
 * buzzer.c
 *
 *  Created on: Sep 29, 2025
 *      Author: RCY
 */

#include "buzzer.h"

// CubeMX가 만든 핸들 외부참조
extern TIM_HandleTypeDef BUZZER_TIM;

// ===== 내부 상태 =====
typedef struct
{
    uint32_t hz;
    uint16_t on_ms;
    uint16_t off_ms;
    uint8_t  repeat;      // 남은 반복
    uint8_t  duty_pct;    // 0~100
} buz_cmd_t;

typedef enum
{
    BZ_IDLE = 0,
    BZ_ON,
    BZ_OFF
} bz_state_t;

#define BZ_QUEUE_LEN   16
static const 	uint16_t   BZ_MAX_BACKLOG_MS = 1200;
static volatile bz_state_t s_state = BZ_IDLE;
static volatile uint16_t   s_left_ms = 0;
static volatile buz_cmd_t  s_cur = {0};
static 			buz_cmd_t  s_q[BZ_QUEUE_LEN];
static volatile uint8_t    s_q_head = 0, s_q_tail = 0;

// 연속음 모드(패턴과 별개): true면 항상 PWM 유지
static volatile bool       s_tone_hold = false;
static volatile uint32_t   s_tone_hold_hz = 0;
static volatile uint8_t    s_tone_hold_duty = 50;

// ===== 유틸 =====
static inline bool q_empty(void)
{
    return s_q_head == s_q_tail;
}

void buzzer_play_adj_left_enter(void)
{
    (void)buzzer_tone_pattern(1000, 70, 20, 1, 55);
    (void)buzzer_tone_pattern(1400, 70,  0, 1, 55);
    (void)buzzer_tone_pattern(1100, 100, 0, 1, 50);
}

void buzzer_play_adj_right_enter(void)
{
    (void)buzzer_tone_pattern(1600, 70, 20, 1, 55);
    (void)buzzer_tone_pattern(1900, 70,  0, 1, 55);
    (void)buzzer_tone_pattern(1700, 100, 0, 1, 50);
}

void buzzer_play_adj_fwd_enter(void)
{
    (void)buzzer_tone_pattern(1300, 70, 20, 1, 55);
    (void)buzzer_tone_pattern(1600, 70,  0, 1, 55);
    (void)buzzer_tone_pattern(1300, 100, 0, 1, 50);
}

void buzzer_play_adj_exit(void)
{
    (void)buzzer_tone_pattern(1800, 60, 20, 1, 50);
    (void)buzzer_tone_pattern(1400, 60, 20, 1, 50);
    (void)buzzer_tone_pattern(1000, 120, 0, 1, 45);
}
static inline bool q_full(void)
{
    return (uint8_t)((s_q_head + 1U) % BZ_QUEUE_LEN) == s_q_tail;
}
static uint32_t cmd_duration_ms(const buz_cmd_t *c);
static uint32_t q_backlog_ms(void);
static bool q_push(buz_cmd_t c)
{
    if (!q_empty())
    {
        uint8_t last = (uint8_t)((s_q_head + BZ_QUEUE_LEN - 1U) % BZ_QUEUE_LEN);//push해서 head+1 되는 거임
        //똑같은 부저 소리 나는 거 막는 게 여기임 (만약 내가 delete 스위치 100번 연타하면 막는 곳이 여기)
        const buz_cmd_t *p = &s_q[last];
        if (p->hz == c.hz && p->on_ms == c.on_ms && p->off_ms == c.off_ms && p->repeat == c.repeat && p->duty_pct == c.duty_pct)
        {
            return true;
        }
    }
    uint32_t new_dur = cmd_duration_ms(&c);	//한 큐의 duration
    uint32_t backlog = q_backlog_ms();		//총 큐의 duration
    //backlog의 최대 시간을 1200ms로 설정 (이건 바꿔도 됨)
    //이 아래 코드의 목적:
    //- 만약 내가 서로 다른 부저음(ex. delete, resume)을 연달아 100번 누름
    //- 그러면 그 경우엔 위 코드에서 막은 중첩과 다르게 서로 다른 음을 연달아서 계속 재생하기 때문에
    //- backlog 상한선을 두고 부저 음을 오래 지속하지 않게 하기 위해 추가한 거임
    while ((backlog + new_dur) > BZ_MAX_BACKLOG_MS && !q_empty())
    {
        backlog -= cmd_duration_ms(&s_q[s_q_tail]);
        s_q_tail = (uint8_t)((s_q_tail + 1U) % BZ_QUEUE_LEN);
    }
    if ((backlog + new_dur) > BZ_MAX_BACKLOG_MS)
    {
        if (c.repeat > 1) c.repeat = 1;
        c.off_ms = 0;
        if (c.on_ms > BZ_MAX_BACKLOG_MS) c.on_ms = (uint16_t)BZ_MAX_BACKLOG_MS;
    }
    while (q_full())
    {
        s_q_tail = (uint8_t)((s_q_tail + 1U) % BZ_QUEUE_LEN);
    }
    s_q[s_q_head] = c;
    s_q_head = (uint8_t)((s_q_head + 1U) % BZ_QUEUE_LEN);
    return true;
}
static bool q_pop(buz_cmd_t *out)
{
    if (q_empty()) return false;
    *out = s_q[s_q_tail];
    s_q_tail = (uint8_t)((s_q_tail + 1U) % BZ_QUEUE_LEN);
    return true;
}

static uint32_t clamp_u32(uint32_t v, uint32_t lo, uint32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint32_t cmd_duration_ms(const buz_cmd_t *c)
{
    if (c->repeat == 0U) return 0U;
    uint32_t rep = (uint32_t)c->repeat;
    uint32_t on = (uint32_t)c->on_ms;
    uint32_t off = (uint32_t)c->off_ms;
    uint32_t gaps = (rep > 0U) ? (rep - 1U) : 0U;
    return on * rep + off * gaps;
}

static uint32_t q_backlog_ms(void)
{
    uint32_t sum = 0U;
    uint8_t i = s_q_tail;
    while (i != s_q_head)
    {
        sum += cmd_duration_ms(&s_q[i]);
        i = (uint8_t)((i + 1U) % BZ_QUEUE_LEN);
    }
    return sum;
}

// ARR/CCR 계산 및 적용
static void pwm_apply(uint32_t hz, uint8_t duty_pct)
{
    hz = clamp_u32(hz, BUZZER_MIN_HZ, BUZZER_MAX_HZ);
    duty_pct = clamp_u8(duty_pct, 1, 99); // 0/100은 완전저/완전고정 → 피함

    // f_tim = 1MHz → ARR = f_tim / f - 1
    uint32_t arr = (BUZZER_TIMER_CLK_HZ / hz);
    if (arr == 0) arr = 1;
    arr -= 1U;
    if (arr > 0xFFFFU) arr = 0xFFFFU;

    uint32_t period = arr + 1U;
    uint32_t ccr = (period * duty_pct) / 100U;
    if (ccr == 0) ccr = 1;                 // 최소 펄스 보장
    if (ccr >= period) ccr = period - 1U;  // 최대 펄스 제한

    // 안전하게 채널 정지 후 갱신
    HAL_TIM_PWM_Stop(&BUZZER_TIM, BUZZER_TIM_CHANNEL);

    __HAL_TIM_SET_AUTORELOAD(&BUZZER_TIM, (uint32_t)arr);
    __HAL_TIM_SET_COMPARE(&BUZZER_TIM, BUZZER_TIM_CHANNEL, (uint32_t)ccr);

    // 갱신 이벤트 발생(ARR 버퍼 적용)
    HAL_TIM_GenerateEvent(&BUZZER_TIM, TIM_EVENTSOURCE_UPDATE);

    HAL_TIM_PWM_Start(&BUZZER_TIM, BUZZER_TIM_CHANNEL);
}

static void pwm_stop(void)
{
    HAL_TIM_PWM_Stop(&BUZZER_TIM, BUZZER_TIM_CHANNEL);
}

// ===== 퍼블릭 구현 =====
void buzzer_init_pwm(void)
{
    // CubeMX에서 TIM16 초기화 끝났다고 가정
    s_state = BZ_IDLE;
    s_left_ms = 0;
    s_q_head = s_q_tail = 0;
    s_tone_hold = false;
    s_tone_hold_hz = 0;
    s_tone_hold_duty = 50;
    pwm_stop();
}

void buzzer_stop(void)
{
    // 패턴/연속음 모두 정지
    s_state = BZ_IDLE;
    s_left_ms = 0;
    s_q_head = s_q_tail = 0;
    s_tone_hold = false;
    pwm_stop();
}

bool buzzer_is_busy(void)
{
    return s_tone_hold || (s_state != BZ_IDLE) || !q_empty();
}

uint8_t buzzer_queue_free(void)
{
    uint8_t used = (s_q_head >= s_q_tail)
                 ? (s_q_head - s_q_tail)
                 : (uint8_t)(BZ_QUEUE_LEN - (s_q_tail - s_q_head));
    return (uint8_t)(BZ_QUEUE_LEN - 1U - used);
}

bool buzzer_tone_start(uint32_t hz, uint8_t duty_pct)
{
    pwm_apply(hz, duty_pct);
    s_tone_hold = true;
    s_tone_hold_hz = hz;
    s_tone_hold_duty = duty_pct;
    return true;
}

void buzzer_tone_stop(void)
{
    s_tone_hold = false;
    s_tone_hold_hz = 0;
    pwm_stop();
}

bool buzzer_tone_once(uint32_t hz, uint16_t on_ms, uint8_t duty_pct)
{
    if (on_ms == 0) return true;
    buz_cmd_t c = (buz_cmd_t){ .hz = hz, .on_ms = on_ms, .off_ms = 0, .repeat = 1, .duty_pct = duty_pct };
    return q_push(c);
}

bool buzzer_tone_pattern(uint32_t hz, uint16_t on_ms, uint16_t off_ms,
                         uint8_t repeat, uint8_t duty_pct)
{
    if (repeat == 0) return true;
    buz_cmd_t c = (buz_cmd_t){ .hz = hz, .on_ms = on_ms, .off_ms = off_ms, .repeat = repeat, .duty_pct = duty_pct };
    return q_push(c);
}

void buzzer_update_1ms(void)
{
    // 연속음 모드면 패턴 무시하고 PWM 유지
    if (s_tone_hold)
    {
        // 혹시 외부에서 TIM이 멈췄다면 재시작 보정(옵션)
        // HAL_TIM_PWM_StateTypeDef 등으로 점검 가능하지만 비용 아껴 패스
        return;
    }

    switch (s_state)
    {
        case BZ_IDLE:
        {
            if (!q_pop((buz_cmd_t *)&s_cur))
            {
                // 대기
                pwm_stop();
                return;
            }
            // ON 구간 시작
            pwm_apply(s_cur.hz, s_cur.duty_pct);
            s_left_ms = s_cur.on_ms;
            s_state = BZ_ON;
            break;
        }

        case BZ_ON:
        {
            if (s_left_ms > 0) s_left_ms--;
            if (s_left_ms == 0)
            {
                // OFF 구간으로
                pwm_stop();
                s_left_ms = s_cur.off_ms;
                s_state = BZ_OFF;

                // 바로 쉬는 시간이 0이면 반복 카운트 처리로 진행
                if (s_left_ms == 0)
                {
                    if (s_cur.repeat > 0) s_cur.repeat--;
                    if (s_cur.repeat > 0)
                    {
                        // 다음 사이클 ON
                        pwm_apply(s_cur.hz, s_cur.duty_pct);
                        s_left_ms = s_cur.on_ms;
                        s_state = BZ_ON;
                    }
                    else
                    {
                        s_state = BZ_IDLE;
                    }
                }
            }
            break;
        }

        case BZ_OFF:
        {
            if (s_left_ms > 0) s_left_ms--;
            if (s_left_ms == 0)
            {
                if (s_cur.repeat > 0) s_cur.repeat--;
                if (s_cur.repeat > 0)
                {
                    // 다음 사이클 ON
                    pwm_apply(s_cur.hz, s_cur.duty_pct);
                    s_left_ms = s_cur.on_ms;
                    s_state = BZ_ON;
                }
                else
                {
                    s_state = BZ_IDLE;
                }
            }
            break;
        }
    }
}

void buzzer_play_pororororong(void)
{
    // 포르테: 짧게 위로 스텝업하면서 “또르르르” 느낌 + 긴 테일
    // 각 스텝: on=60ms, off=20ms (마지막 스텝만 off=0)
    // 전체 길이 ≈ 7*(60+20) + 300 = 860 ms
    // 큐 사용: 8개 (7스텝 + 테일 1개) → BZ_QUEUE_LEN=8에 맞춤

    (void)buzzer_tone_pattern( 900,  60, 20, 1, 50);  // poro-
    (void)buzzer_tone_pattern(1050,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1250,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1450,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1700,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1950,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(2200,  60,  0, 1, 50);  // 마지막 또르르 끝

    // 롱— (조금 낮춰서 1.1kHz로 300ms sustain)
//    (void)buzzer_tone_pattern(1100, 300,  0, 1, 50);
}

void buzzer_play_shutdown_pororororong(void)
{
    // 먼저 다른 소리(루프 포함) 전부 정리
    buzzer_stop();

    // 상승 버전과 동일한 길이/리듬을 그대로 뒤집음
    // 각 스텝: on=60ms, off=20ms (마지막 스텝만 off=0)
    // 총 큐 사용 8개(7스텝 + 테일 1개) → 기본 BZ_QUEUE_LEN=8에 맞춤

    (void)buzzer_tone_pattern(2200,  60, 20, 1, 50);  // 롱의 시작을 높은 톤에서
    (void)buzzer_tone_pattern(1950,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1700,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1450,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1250,  60, 20, 1, 50);
    (void)buzzer_tone_pattern(1050,  60, 20, 1, 50);
    (void)buzzer_tone_pattern( 900,  60,  0, 1, 50);  // 또르르 마무리

    // 마지막 “롱—”은 더 낮게 700Hz로 300ms 유지 (살짝 잔향 느낌)
//    (void)buzzer_tone_pattern( 700, 300,  0, 1, 45);
}

void buzzer_play_resume(void)
{
    // 딩-딩-딩↗ + 살짝 길게 끝: 총 ~520ms
    // on/off는 짧고 경쾌, 주파수는 점점 상승
    (void)buzzer_tone_pattern( 900,  80, 30, 1, 50);
    (void)buzzer_tone_pattern(1200,  80, 30, 1, 50);
    (void)buzzer_tone_pattern(1500, 100, 40, 1, 50);
    (void)buzzer_tone_pattern(1800, 140,  0, 1, 50);  // 마무리 롱
}

void buzzer_play_execute(void)
{
    // 타-타-타아— : 0.07s, 0.07s, 0.30s (사이 40ms 갭)
    // 총 ~520ms, 상승(0.9→1.2→1.6kHz), duty 55%
    (void)buzzer_tone_pattern( 900,  70, 40, 1, 55);
    (void)buzzer_tone_pattern(1200,  70, 40, 1, 55);
    (void)buzzer_tone_pattern(1600, 300,  0, 1, 55);
}


void buzzer_play_birik(void)
{
    // “삐↗릭” : 짧게 올렸다가 살짝 내려오며 마무리 (~165 ms)
    // 큐 사용 3칸
    (void)buzzer_tone_pattern(1400, 40, 10, 1, 55);
    (void)buzzer_tone_pattern(1900, 55,  0, 1, 55);
    (void)buzzer_tone_pattern(1100, 70,  0, 1, 50);
}

void buzzer_play_biriririring(void)
{
    // 큐 여유가 부족하면 간단 버전으로 대체
    if (buzzer_queue_free() < 15)
    {
        // 최소 보장: 1.4 kHz 250 ms
        (void)buzzer_tone_once(1400, 250, 50);
        return;
    }

    // “삐리리리링”: 빠른 트릴(주파수 교차) 6스텝 + 롱 테일
    // 각 스텝: on=40ms, off=10ms, duty=50~55%
    // 총 길이 ≈ 6*(40+10) + 300 = 600 ms
    (void)buzzer_tone_pattern(1300, 40, 10, 1, 50);
    (void)buzzer_tone_pattern(1600, 40, 10, 1, 55);
    (void)buzzer_tone_pattern(1350, 40, 10, 1, 50);
    (void)buzzer_tone_pattern(1650, 40, 10, 1, 55);
    (void)buzzer_tone_pattern(1400, 40, 10, 1, 50);
    (void)buzzer_tone_pattern(1700, 40,  0, 1, 55); // 트릴 끝

    // 링— 테일
    (void)buzzer_tone_pattern(1400, 300, 0, 1, 50);
}

void buzzer_play_no_index(void)
{
    // “뚝-뚝↓” : 하강 두음 → ‘없음/불가’ 직관적
    // 800 Hz 120ms, 60ms 쉼 → 600 Hz 220ms
    (void)buzzer_tone_pattern( 800, 120, 60, 1, 45);
    (void)buzzer_tone_pattern( 600, 220,  0, 1, 45);
}

// ↑: 짧은 상승 두음 (상향 느낌)
void buzzer_play_input_up(void)
{
    (void)buzzer_tone_pattern(1200, 40, 10, 1, 50);
    (void)buzzer_tone_pattern(1500, 60,  0, 1, 50);
}

// ↓: 짧은 하강 두음 (하향 느낌)
void buzzer_play_input_down(void)
{
    (void)buzzer_tone_pattern(1500, 40, 10, 1, 50);
    (void)buzzer_tone_pattern(1200, 60,  0, 1, 50);
}

// ←: 저음 단음(좌, 낮은 피치)
void buzzer_play_input_left(void)
{
    (void)buzzer_tone_pattern( 900, 70, 0, 1, 45);
}

// →: 고음 단음(우, 높은 피치)
void buzzer_play_input_right(void)
{
    (void)buzzer_tone_pattern(1800, 70, 0, 1, 45);
}

void buzzer_play_dir_click(void)
{
    // 경쾌한 단음 클릭
    (void)buzzer_tone_pattern(1400, 70, 0, 1, 45);
}

void buzzer_play_dir_click_soft(void)
{
    // 조용한 환경용 소프트 클릭
    (void)buzzer_tone_pattern(1200, 60, 0, 1, 35);
}

void buzzer_play_repeat1(void)
{
    // 1회: 약간 길고 낮은 한 방 (명확하게 한 번만)
	(void)buzzer_tone_pattern(1200, 90, 130, 2, 48);
}

void buzzer_play_repeat2(void)
{
    // 2회: 동일 톤 두 번, 명확한 간격
    // on=90ms, off=130ms × 2
//	(void)buzzer_tone_pattern(1400, 70, 90, 3, 45);

	(void)buzzer_tone_pattern(1200, 90, 130, 3, 48);
}

void buzzer_play_repeat3(void)
{
    // 3회: 조금 더 짧고 빠른 삼연속
    // on=70ms, off=90ms × 3
//    (void)buzzer_tone_pattern(1600, 50, 70, 4, 42);

    (void)buzzer_tone_pattern(1200, 90, 130, 4, 48);
}

void buzzer_play_calib_enter(void)
{
    // 밝은 상승 3음 + 살짝 길게 마무리 (총 ~430ms)
    // 1.0kHz 70ms → 1.3kHz 70ms → 1.6kHz 90ms(마무리)
    (void)buzzer_tone_pattern(1000, 70, 20, 1, 50);
    (void)buzzer_tone_pattern(1300, 70, 20, 1, 50);
    (void)buzzer_tone_pattern(1600, 90,  0, 1, 50);
}

void buzzer_start_calib_heartbeat(void)
{
    // 모드 유지 알림: 600ms 주기, 1.2kHz 35ms / 565ms off
    // 너무 거슬리지 않게 duty 30%
    (void)buzzer_tone_pattern(1200, 35, 565, 1, 30);
}

void buzzer_stop_calib_heartbeat(void)
{
    // 큐/소리 모두 정지
    buzzer_stop();
}

void buzzer_play_calib_done(void)
{
    // 하트비트 등 남아있을 수 있으니 먼저 정지
    buzzer_stop();

    // 밝은 장조 느낌: 1.05k → 1.32k → 1.65k 짧게 올리고
    // 마지막 2.1kHz를 약간 길게 “완료!” (총 ~470 ms)
    (void)buzzer_tone_pattern(1050, 60, 20, 1, 50);
    (void)buzzer_tone_pattern(1320, 60, 20, 1, 50);
    (void)buzzer_tone_pattern(1650, 70, 20, 1, 50);
    (void)buzzer_tone_pattern(2100, 160, 0, 1, 50);
}

void buzzer_play_calib_done_soft(void)
{
    // 볼륨/길이 모두 살짝 줄인 소프트 완료음 (총 ~360 ms)
    (void)buzzer_tone_pattern(1050, 45, 15, 1, 35);
    (void)buzzer_tone_pattern(1320, 45, 15, 1, 35);
    (void)buzzer_tone_pattern(1650, 55, 15, 1, 35);
    (void)buzzer_tone_pattern(2000, 120, 0, 1, 35);
}

// 3) Low “chime” (two low notes, smooth & calm)
void buzzer_warn_soft_low_chime(void)
{
    // 700 → 820 Hz, short/soft
	(void)buzzer_tone_pattern(700, 40, 20,  1, 22);
	(void)buzzer_tone_pattern(820, 50,  0,  1, 22);
}

// single low “bonk”: ultra minimal (~90 ms total)
void buzzer_play_buffer_full_bonk(void)
{
    (void)buzzer_tone_pattern(480, 90, 0, 1, 26);
}

// === 2) START/RESTART: 타-타-타아 (~380 ms), 조금 더 힘있게 ===
void buzzer_play_calib_start_go(void)
{
    // 아주 부드럽게 (900 → 1150 → 1400 Hz), 더 낮은 duty
    (void)buzzer_tone_pattern( 900, 45, 20, 1, 26);
    (void)buzzer_tone_pattern(1150, 45, 20, 1, 26);
    (void)buzzer_tone_pattern(1400, 95,  0, 1, 24);

    // 미세 무음으로 마감
    (void)buzzer_tone_pattern(900, 0, 10, 1, 14);
}

void buzzer_play_jig_mode(void)
{
    buzzer_stop();
    (void)buzzer_tone_pattern(1200, 70, 30, 1, 55);
    (void)buzzer_tone_pattern(2000, 70, 30, 1, 55);
    (void)buzzer_tone_pattern(1200, 70, 30, 1, 55);
    (void)buzzer_tone_pattern(2000, 70, 30, 1, 55);
    (void)buzzer_tone_pattern(1200, 70, 30, 1, 55);
    (void)buzzer_tone_pattern(2000, 70, 30, 1, 55);
    (void)buzzer_tone_pattern(1600, 220, 0, 1, 55);
}

void buzzer_play_jig_confirm(void)
{
    (void)buzzer_tone_pattern(1100, 60, 20, 1, 48);
    (void)buzzer_tone_pattern(1400, 70, 20, 1, 50);
    (void)buzzer_tone_pattern(1800, 90,  0, 1, 50);
}

void buzzer_play_jig_done(void)
{
    (void)buzzer_tone_pattern(900,  60, 20, 1, 45);
    (void)buzzer_tone_pattern(1200, 70, 20, 1, 48);
    (void)buzzer_tone_pattern(1600, 160, 0, 1, 50);
}

void buzzer_play_jig_item1_cue(void)
{
    (void)buzzer_tone_pattern(1400, 100, 0, 1, 50);
}

void buzzer_play_jig_item2_cue(void)
{
	(void)buzzer_tone_pattern(1400, 100, 100, 3, 50);
}

void buzzer_play_jig_item3_cue(void)
{
	(void)buzzer_tone_pattern(1400, 100, 100, 2, 50);
}

void buzzer_play_jig_clear(void)
{
    (void)buzzer_tone_pattern(1100, 60, 20, 1, 48);
    (void)buzzer_tone_pattern(1400, 60, 20, 1, 50);
    (void)buzzer_tone_pattern(1750, 120, 0, 1, 52);
}

void buzzer_play_jig_fail(void)
{
    buzzer_stop();
    (void)buzzer_tone_pattern(1800, 90, 50, 1, 55);
    (void)buzzer_tone_pattern(1200, 90, 50, 1, 55);
    (void)buzzer_tone_pattern( 700, 250,  0, 1, 55);
}

void buzzer_play_bootloader_enter(void)
{
	buzzer_stop();
	(void)buzzer_tone_pattern(1000, 80, 30, 1, 50);
	(void)buzzer_tone_pattern(1400, 80, 30, 1, 55);
	(void)buzzer_tone_pattern(1800, 80, 60, 1, 60);
	(void)buzzer_tone_pattern( 900, 220,  0, 1, 45);
}

void buzzer_play_recognition_on(void)
{
    // 인식 시작: 짧고 또렷한 상승 3음 (~200 ms)
    (void)buzzer_tone_pattern(1100, 50, 20, 1, 50);
    (void)buzzer_tone_pattern(1450, 60, 20, 1, 52);
    (void)buzzer_tone_pattern(1800, 90,  0, 1, 52);
}


void buzzer_play_cosmic_shutdown(void)
{
    // 즉시 정지 후 시작
    buzzer_stop();

    // 1~3 스텝: 고음에서 신호가 끊기기 시작하는 페이드아웃 도입 (각 20ms)
    (void)buzzer_tone_pattern(3200, 20, 0, 1, 45);
    (void)buzzer_tone_pattern(2700, 20, 0, 1, 45);
    (void)buzzer_tone_pattern(2200, 20, 0, 1, 45);

    // 4~7 스텝: 중력에 이끌려 빠르게 하강 (각 25ms)
    (void)buzzer_tone_pattern(1800, 25, 0, 1, 48);
    (void)buzzer_tone_pattern(1450, 25, 0, 1, 50);
    (void)buzzer_tone_pattern(1150, 25, 0, 1, 50);
    (void)buzzer_tone_pattern( 900, 30, 0, 1, 50);

    // 8~11 스텝: 무한한 심연으로 가라앉듯 톤이 묵직해지며 늘어짐 (각 45ms)
    (void)buzzer_tone_pattern( 700, 45, 0, 1, 45);
    (void)buzzer_tone_pattern( 550, 50, 0, 1, 40);
    (void)buzzer_tone_pattern( 430, 60, 0, 1, 30);
    (void)buzzer_tone_pattern( 350, 80, 0, 1, 15); // 최저음에서 소멸

    // 마지막에 완벽한 무음 갭을 줘서 안전하게 끝맺음
    (void)buzzer_tone_pattern( 100,  0, 50, 1,  1);
}

void buzzer_play_cosmic_boot(void)
{
    // 이전 소리 정리
    buzzer_stop();

    // 1~4 스텝: 저음역대에서 에너지가 모이는 부드러운 시작 (각 35ms)
    (void)buzzer_tone_pattern( 600, 35, 0, 1, 40);
    (void)buzzer_tone_pattern( 650, 35, 0, 1, 42);
    (void)buzzer_tone_pattern( 720, 35, 0, 1, 45);
    (void)buzzer_tone_pattern( 820, 35, 0, 1, 48);

    // 5~9 스텝: 차원이 열리며 급격하게 치솟는 중음역대 스윕 (각 20ms로 속도 업)
    (void)buzzer_tone_pattern( 970, 20, 0, 1, 50);
    (void)buzzer_tone_pattern(1180, 20, 0, 1, 50);
    (void)buzzer_tone_pattern(1450, 20, 0, 1, 50);
    (void)buzzer_tone_pattern(1800, 20, 0, 1, 50);
    (void)buzzer_tone_pattern(2250, 20, 0, 1, 50);

    // 10~12 스텝: 우주 공간에 도달한 초고음 하이라이트 (각 15ms)
    (void)buzzer_tone_pattern(2800, 15, 0, 1, 45);
    (void)buzzer_tone_pattern(3400, 15, 0, 1, 40);
    (void)buzzer_tone_pattern(3900, 25, 0, 1, 35); // 피크 점

    // 마무리: 신비롭게 퍼지는 우주 잔향 (초고음에서 살짝 꺾인 2.4kHz로 부드럽게 롱~ 유지)
    (void)buzzer_tone_pattern(2400, 250, 0, 1, 30); // Duty를 낮춰 부드러운 여운 생성
}


void buzzer_play_cosmic_boot_soft(void)
{
    buzzer_stop();

    // 1~4 스텝: 바닥에서 웅- 하고 맴도는 묵직한 저음 (Duty 35~40%로 부드럽게)
    (void)buzzer_tone_pattern( 550, 40, 0, 1, 35);
    (void)buzzer_tone_pattern( 620, 40, 0, 1, 38);
    (void)buzzer_tone_pattern( 700, 35, 0, 1, 40);
    (void)buzzer_tone_pattern( 800, 35, 0, 1, 40);

    // 5~8 스텝: 매끄럽게 상승하는 중음역대 (Duty를 35%로 고정하여 톤을 일정하게 유지)
    (void)buzzer_tone_pattern( 950, 25, 0, 1, 35);
    (void)buzzer_tone_pattern(1150, 25, 0, 1, 35);
    (void)buzzer_tone_pattern(1400, 25, 0, 1, 35);
    (void)buzzer_tone_pattern(1700, 25, 0, 1, 30);

    // 9~11 스텝: 최고음 대역이지만 Duty를 25%로 낮춰 소리를 얇고 둥글게 만듦
    (void)buzzer_tone_pattern(2000, 20, 0, 1, 25);
    (void)buzzer_tone_pattern(2200, 25, 0, 1, 25); // 피크 주파수를 2.2kHz로 제한

    // 마무리: 공중에 부유하는 듯한 부드럽고 신비로운 웅— (Sustain)
    (void)buzzer_tone_pattern(1500, 210, 0, 1, 25);
}

void buzzer_play_cosmic_shutdown_soft(void)
{
    buzzer_stop();

    // 1~3 스텝: 고음부 도입을 아주 부드럽게 시작 (Duty 25%)
    (void)buzzer_tone_pattern(2100, 25, 0, 1, 25);
    (void)buzzer_tone_pattern(1850, 25, 0, 1, 25);
    (void)buzzer_tone_pattern(1600, 25, 0, 1, 28);

    // 4~7 스텝: 부드럽게 감속하며 낙하
    (void)buzzer_tone_pattern(1350, 30, 0, 1, 30);
    (void)buzzer_tone_pattern(1100, 30, 0, 1, 35);
    (void)buzzer_tone_pattern( 900, 35, 0, 1, 35);
    (void)buzzer_tone_pattern( 750, 35, 0, 1, 38);

    // 8~10 스텝: 안개 속으로 사라지듯 톤이 깊어지며 소멸
    (void)buzzer_tone_pattern( 620, 50, 0, 1, 40);
    (void)buzzer_tone_pattern( 500, 60, 0, 1, 35);
    (void)buzzer_tone_pattern( 400, 70, 0, 1, 20); // 마지막은 힘을 빼고 스르륵
    (void)buzzer_tone_pattern( 300, 110, 0, 1, 10);

    // 안전 마감 무음 갭
    (void)buzzer_tone_pattern( 100,  0, 50, 1,  1);
}

void buzzer_play_cosmic_boot_v2(void)
{
    buzzer_stop();

    // 1~3 스텝: 깊은 우주 속동력원이 켜지는 신호 (낮은 중음에서 시작)
    (void)buzzer_tone_pattern( 880, 40, 0, 1, 35); // A5 음역대 기준 부드러운 시작
    (void)buzzer_tone_pattern(1046, 40, 0, 1, 35); // C6 도달
    (void)buzzer_tone_pattern(1318, 50, 0, 1, 30); // E6 도달 (조화로운 3도 화음 스윕)

    // 4~7 스텝: 에너지가 부드러운 곡선을 그리며 하강 (날카로움을 없애는 반전 연출)
    (void)buzzer_tone_pattern(1174, 30, 0, 1, 30);
    (void)buzzer_tone_pattern( 987, 30, 0, 1, 35);
    (void)buzzer_tone_pattern( 880, 30, 0, 1, 40);

    // 마무리: 저음역대에서 묵직하고 따뜻하게 공간을 채우는 웅— (Sustain)
    // 587Hz(D5) 부근에서 Duty를 25%로 낮춰 멀리서 울리는 듯한 황홀한 잔향을 만듭니다.
    (void)buzzer_tone_pattern( 587, 370, 0, 1, 25);
}

void buzzer_play_cosmic_shutdown_v2(void)
{
    buzzer_stop();

    // 1~4 스텝: 공간이 수축하기 시작하는 단계 (중고음에서 매끄럽게 하강)
    (void)buzzer_tone_pattern(1567, 30, 0, 1, 25); // 1.5kHz 대역의 부드러운 시작
    (void)buzzer_tone_pattern(1396, 30, 0, 1, 25);
    (void)buzzer_tone_pattern(1174, 35, 0, 1, 30);
    (void)buzzer_tone_pattern( 987, 35, 0, 1, 30);

    // 5~8 스텝: 한 점으로 빨려 들어가며 음이 급격하게 쪼개짐
    (void)buzzer_tone_pattern( 784, 40, 0, 1, 35);
    (void)buzzer_tone_pattern( 659, 45, 0, 1, 30);
    (void)buzzer_tone_pattern( 523, 55, 0, 1, 20); // 여기서부터 힘이 급격히 빠짐

    // 9~10 스텝: 마지막 스파크가 튀듯 완전히 수축하며 소멸 (초저음 + 최소 Duty)
    (void)buzzer_tone_pattern( 392, 70, 0, 1, 10);
    (void)buzzer_tone_pattern( 261, 110, 0, 1, 5); // 261Hz(가온 도)의 아주 미약한 소리로 마감

    // 안전 마감 무음 갭
    (void)buzzer_tone_pattern( 100,  0, 50, 1,  1);
}

void buzzer_play_bootloader_cosmic(void)
{
    buzzer_stop();

    // 1~3 스텝: 경고인 듯 신비로운 낯선 공간의 노크 소리 (짧고 묘한 리듬)
    (void)buzzer_tone_pattern( 987, 40, 20, 1, 35); // 시(B5) 단음 틱-
    (void)buzzer_tone_pattern(1479, 50, 40, 1, 30); // 파#(F#6) 높은 음 툭-- (신비로운 5도 음정)

    // 4~8 스텝: 시스템이 미지의 코드를 읽어 내려가듯 주파수가 출렁이는 구간
    (void)buzzer_tone_pattern(1109, 30, 0, 1, 35);
    (void)buzzer_tone_pattern(1244, 30, 0, 1, 35);
    (void)buzzer_tone_pattern(1109, 30, 0, 1, 35);
    (void)buzzer_tone_pattern( 987, 45, 0, 1, 35);
    (void)buzzer_tone_pattern( 880, 45, 0, 1, 40);

    // 9~11 스텝: 게이트가 열리며 미지의 블랙홀 내부로 진입 (주파수가 바닥으로 급강하)
    (void)buzzer_tone_pattern( 698, 50, 0, 1, 40);
    (void)buzzer_tone_pattern( 554, 60, 0, 1, 35);

    // 마무리 테일: 차원의 밑바닥에 도착하여 울리는 기괴하고 웅장한 공동(Cavity) 사운드
    // 아주 낮은 349Hz(F4) 음을 길게 빼서 "여긴 전혀 다른 공간이다"라는 시각적 착각을 줍니다.
    (void)buzzer_tone_pattern( 349, 320, 0, 1, 20);
}

void buzzer_play_space_enter(void)
{
    buzzer_stop();

    (void)buzzer_tone_pattern(1000, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1300, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1600, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1400, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1800, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1600, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1900, 120, 0, 1, 30);
}

void buzzer_play_space_exit(void)
{
    buzzer_stop();

    (void)buzzer_tone_pattern(1800, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1500, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1700, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1300, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1500, 45, 10, 1, 40);
    (void)buzzer_tone_pattern(1100, 45, 10, 1, 40);

    (void)buzzer_tone_pattern(1300, 100, 0, 1, 25);
}

void buzzer_play_cosmic_boot_low(void)
{
    buzzer_stop();

    (void)buzzer_tone_pattern(280, 120, 10, 1, 18);
    (void)buzzer_tone_pattern(340, 100, 10, 1, 20);
    (void)buzzer_tone_pattern(420, 100, 10, 1, 22);
    (void)buzzer_tone_pattern(520, 120, 10, 1, 24);

    // 우주 게이트 진입
    (void)buzzer_tone_pattern(680, 300, 0, 1, 20);
}

void buzzer_play_cosmic_shutdown_low(void)
{
    buzzer_stop();

    (void)buzzer_tone_pattern(680, 120, 10, 1, 20);
    (void)buzzer_tone_pattern(520, 100, 10, 1, 22);
    (void)buzzer_tone_pattern(420, 100, 10, 1, 20);
    (void)buzzer_tone_pattern(340, 120, 10, 1, 18);

    // 엔진 정지
    (void)buzzer_tone_pattern(260, 350, 0, 1, 12);
}


void buzzer_play_bootloader_blackhole(void)
{
    buzzer_stop();

    (void)buzzer_tone_pattern(900 , 40, 10, 1, 35);
    (void)buzzer_tone_pattern(1200, 40, 10, 1, 35);
    (void)buzzer_tone_pattern(1000, 40, 10, 1, 35);

    (void)buzzer_tone_pattern(1500, 60, 10, 1, 35);
    (void)buzzer_tone_pattern(1300, 60, 10, 1, 35);

    (void)buzzer_tone_pattern(1700, 80, 10, 1, 30);
    (void)buzzer_tone_pattern(1450, 220, 0, 1, 25);
}

void buzzer_play_calib_enter_v3(void)
{
    buzzer_stop();

    (void)buzzer_tone_pattern(1000, 35, 10, 1, 35);
    (void)buzzer_tone_pattern(1250, 35, 10, 1, 35);
    (void)buzzer_tone_pattern(1450, 35, 10, 1, 35);
    (void)buzzer_tone_pattern(1250, 35, 10, 1, 35);

    (void)buzzer_tone_pattern(1650, 120, 0, 1, 25);
}
