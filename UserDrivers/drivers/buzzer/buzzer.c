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
