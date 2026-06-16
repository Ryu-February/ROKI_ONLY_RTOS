/*
 * lp_stby.h
 *
 *  Created on: Sep 24, 2025
 *      Author: RCY
 */

#ifndef POWER_LP_STBY_H_
#define POWER_LP_STBY_H_

#include "def.h"


// 디바운스/홀드 시간(ms)
#ifndef LP_STBY_DEBOUNCE_MS
#define LP_STBY_DEBOUNCE_MS  		30
#endif

#define LP_STBY_WAKE_HOLD_MS       	300   // 켤 때 필요한 길게-눌림 시간(1s)

#define LP_STBY_BOOT_RC_DELAY_MS   	30     // RC 안정 대기

#ifndef LP_STBY_HOLD_MS
#define LP_STBY_HOLD_MS      		100		// 500ms(100 & 5ms)
#endif

#define LP_STBY_IDLE_TIMEOUT_MS   	1800000U   // 900 s == 15 min | 1800s = 30 min
#define LP_STBY_BEFORE_WARN_MS   	1740000U   // 840 s == 14 min | 1740s = 29 min



void lp_stby_init(void);
void lp_stby_on_5ms(void);         // 1ms 주기에서 호출
void lp_stby_force(void);          // 즉시 Standby 진입


void lp_stby_boot_gate(void);
// 사용자 훅(모터 정지/LED OFF/상태 저장 등). 필요 시 오버라이드.
void lp_stby_prepare_before(void) __attribute__((weak));

void lp_stby_user_activity(void);
uint32_t lp_stby_get_idle_ms(void);
void lp_stby_check(void);
bool lp_power_off(void);


#endif /* POWER_LP_STBY_H_ */
