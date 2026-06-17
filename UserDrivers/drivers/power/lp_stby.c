/*
 * lp_stby.c
 *
 *  Created on: Sep 24, 2025
 *      Author: RCY
 */


#include "lp_stby.h"


// ==== 하드웨어 핀 ====
// PB1에 Delete 스위치 (프로젝트 기준)
#define LP_STBY_BTN_PORT     GPIOA
#define LP_STBY_BTN_PIN      GPIO_PIN_0

// 내부 상태
static bool     s_fired          = false;
static inline bool btn_pressed_raw(void)
{
    // Active-Low: 눌림 → LOW
    return (HAL_GPIO_ReadPin(LP_STBY_BTN_PORT, LP_STBY_BTN_PIN) == GPIO_PIN_RESET);
}

static inline bool was_from_standby(void)
{
#if defined(PWR_WUSR_SBF)
    return (READ_BIT(PWR->WUSR, PWR_WUSR_SBF) != 0U);
#elif defined(PWR_SR1_SBF)
    return (READ_BIT(PWR->SR1, PWR_SR1_SBF) != 0U);
#else
    return false;
#endif
}

static inline void ensure_wkup1_off_at_boot(void)
{
#ifdef PWR_WUCR1_WUPEN1
    CLEAR_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);
#endif
#ifdef PWR_WUSR_WUF1
    WRITE_REG(PWR->WUSR, PWR_WUSR_WUF1);  // 잔여 Wake flag 정리
#endif
}

static inline void clear_standby_flag(void)
{
#if defined(PWR_WUSR_SBF)
    WRITE_REG(PWR->WUSR, PWR_WUSR_SBF);   // write 1 to clear
#elif defined(PWR_SCR_CSBF)
    SET_BIT(PWR->SCR, PWR_SCR_CSBF);
#endif
}

static void reenter_standby_now(void)
{
    // Standby 재진입: WKUP1은 진입 직전에만 Enable (네가 이미 만든 함수 사용)
    // wkup1_config_for_active_low();  // 소스선택/플래그클리어/Enable
    __disable_irq();
    HAL_SuspendTick();
    HAL_PWREx_EnterSHUTDOWNMode();
    while (1)
    {
    }
}


static void wkup1_config_for_active_low(void)
{
    // 0) Disable 먼저
    CLEAR_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);

    // 1) 폴라리티 = LOW 활성
//    SET_BIT(PWR->WUCR3, PWR_WUCR3_WUPP1);
//
//    // 2) 내부 Pull = NOPULL (외부 10 kΩ 사용)
//    MODIFY_REG(PWR->WUCR2, PWR_WUCR2_WUPPUPD1_Msk, (0U << PWR_WUCR2_WUPPUPD1_Pos));

    // 3) Wake flag/WUF1 클리어 (1을 써서 클리어)
    WRITE_REG(PWR->WUSR, PWR_WUSR_WUF1);

    // (선택) SBF도 정리
#ifdef PWR_WUSR_SBF
    WRITE_REG(PWR->WUSR, PWR_WUSR_SBF);
#endif

    // 4) Enable
    SET_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);
}


void lp_stby_prepare_before(void)
{
    // 사용자가 오버라이드:
    // - 모터/버저/LED OFF
    // - 상태 저장(Flash/EEPROM)
    // - 주변장치 안전 종료
}

static void lp_stby_enter(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);//근데 얘 필요 없긴 함(a3919-sleepn 핀인데 있으나 마나임)
    lp_stby_prepare_before();

    __disable_irq();
    HAL_SuspendTick();

    // Wake-up 소스는 CubeMX에서 설정(예: RTC Alarm, WKUP 핀)
    wkup1_config_for_active_low();
    HAL_PWR_EnterSTANDBYMode();

//    while (1)
//    {
//        // 돌아오지 않음
//    }
}

void lp_stby_force(void)
{
    s_fired = true;
    lp_stby_enter();
}

static bool wkup1_arm_for_pa0(void)
{
    // 0) Disable
    CLEAR_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);

    // 1) 소스 선택: WUCR3.WUSEL1을 PA0에 맞는 값으로 설정
//    MODIFY_REG(PWR->WUCR3, PWR_WUCR3_WUSEL1_Msk,
//               (WKUP1_SEL_FOR_PA0 << PWR_WUCR3_WUSEL1_Pos));

    // 2) 플래그 클리어
    WRITE_REG(PWR->WUSR, PWR_WUSR_WUF1);

    // 3) Enable
    SET_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);

    // 4) Enable 직후 플래그 체크 → 활성로 보이면 취소
    if (READ_BIT(PWR->WUSR, PWR_WUSR_WUF1)) {
        CLEAR_BIT(PWR->WUCR1, PWR_WUCR1_WUPEN1);
        WRITE_REG(PWR->WUSR, PWR_WUSR_WUF1);
        return false;
    }
    return true;
}

void enter_standby_safe(void)
{
    // 릴리즈(High) 최종 소프트 체크
//    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) != GPIO_PIN_SET)
//    	return;

    lp_stby_prepare_before();

    if (!wkup1_arm_for_pa0())
    	return;     // 활성로 보이면 진입 취소

    __disable_irq();
    HAL_SuspendTick();
//    HAL_PWR_EnterSTANDBYMode();
    HAL_PWREx_EnterSHUTDOWNMode();
    while (1) { }
}
