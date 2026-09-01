/*-----------------------------------------------------------
 * TIM2_CH2 (PA1) PWM — 패시브 부저 구동 (CLAUDE.md 7장 2단계)
 *
 * 회로: PA1 --[330R]-- B(KSP2222ABU), C -- 부저(-), E -- GND, 부저(+) -- 5V
 * 로우사이드 NPN 스위치이므로 PA1이 High인 동안 부저에 전류가 흐른다.
 *
 * 설계:
 *   PSC로 카운터 틱을 BUZZER_TICK_HZ(1MHz)로 고정하고,
 *   주파수는 ARR만, 세기는 CCR2(duty)만 바꾼다.
 *   -> SetFrequency/SetDuty를 서로 독립적으로 호출할 수 있다.
 *
 * 참조: RM0351 31장 (TIM2/TIM3/TIM4/TIM5)
 *-----------------------------------------------------------*/

#include "stm32l476xx.h"
#include "config.h"
#include "tasks_app.h"

/* 현재 duty(%). SetFrequency로 ARR이 바뀌어도 같은 duty를 유지하기 위해 보관한다.
 * (CCR2는 ARR에 비례하는 절대값이라 ARR만 바꾸면 duty가 틀어진다) */
static uint8_t s_duty_percent = 0;

/* ARR로부터 CCR2를 다시 계산해 넣는다. */
static void tim2_apply_duty(void)
{
    uint32_t period = TIM2->ARR + 1U;          /* 카운터 주기(틱 수) */

    /* PWM mode 1: CNT < CCR2 인 동안 출력 High.
     * duty 0  -> CCR2 = 0      -> 항상 Low  (부저 무음)
     * duty 100 -> CCR2 = period -> 항상 High (DC. 피에조는 '틱' 후 무음이라 경보음으로 쓰지 않음) */
    TIM2->CCR2 = (period * s_duty_percent) / 100U;
}

void TIM2_PWM_Init(void)
{
    /* --- 1. PA1을 TIM2_CH2 대체기능(AF1)으로 --- */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    /* PA1의 MODER 리셋값은 11(아날로그)이므로 반드시 clear 후 set (CLAUDE.md 0.5) */
    GPIOA->MODER &= ~GPIO_MODER_MODE1;
    GPIOA->MODER |= GPIO_MODER_MODE1_1;        /* 10 = Alternate function */

    GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL1;
    GPIOA->AFR[0] |= (1U << GPIO_AFRL_AFSEL1_Pos);   /* AF1 = TIM2_CH2 */

    /* OTYPER = push-pull, OSPEEDR = low, PUPDR = none 은 리셋값 그대로 사용.
     * 수 kHz 구형파에 트랜지스터 베이스 구동이라 low speed로 충분하다. */

    /* --- 2. TIM2 클럭 --- */
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

    /* --- 3. 타임베이스: 카운터 틱을 BUZZER_TICK_HZ로 --- */
    TIM2->PSC = (uint32_t)(TIM2_CLK_HZ / BUZZER_TICK_HZ) - 1U;
    TIM2->ARR = (uint32_t)(BUZZER_TICK_HZ / BUZZER_FREQ_NORMAL_HZ) - 1U;

    /* --- 4. 채널 2를 PWM mode 1 출력으로 --- */
    TIM2->CCMR1 &= ~TIM_CCMR1_CC2S;            /* 00 = 출력 채널 */
    TIM2->CCMR1 &= ~TIM_CCMR1_OC2M;            /* OC2M[3:0] 전체 clear (bit24 포함) */
    TIM2->CCMR1 |= (TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1);   /* 0110 = PWM mode 1 */
    TIM2->CCMR1 |= TIM_CCMR1_OC2PE;            /* CCR2 preload: 갱신이 update 이벤트에 동기화 */

    TIM2->CCER &= ~TIM_CCER_CC2P;              /* active high */
    TIM2->CCER |= TIM_CCER_CC2E;               /* OC2 출력 활성화 */

    TIM2->CR1 |= TIM_CR1_ARPE;                 /* ARR preload. 동작 중 주파수 변경 시 글리치 방지 */

    /* --- 5. duty 0(무음)으로 시작 --- */
    s_duty_percent = 0;
    tim2_apply_duty();

    /* PSC/ARR/CCR은 shadow 레지스터라 update 이벤트가 있어야 실제 반영된다.
     * UG로 강제 발생시켜 초기값을 로드한다. */
    TIM2->EGR |= TIM_EGR_UG;

    TIM2->CR1 |= TIM_CR1_CEN;                  /* 카운터 시작 */
}

/* 경보음 주파수 설정. duty는 유지된다.
 * TIM2는 32비트 타이머라 ARR 범위 걱정은 없지만, freq가 0이면 0除算이므로 막는다. */
void TIM2_SetFrequency(uint16_t freq_hz)
{
    if (freq_hz == 0U) {
        return;
    }

    TIM2->ARR = (uint32_t)(BUZZER_TICK_HZ / freq_hz) - 1U;
    tim2_apply_duty();                         /* 새 ARR 기준으로 CCR2 재계산 */
}

/* 경보음 세기(duty). 0 = 무음, 50 = 표준 구형파.
 * 피에조 부저는 duty 50%에서 음압이 가장 크고 0/100에 가까울수록 작아진다. */
void TIM2_SetDuty(uint8_t duty_percent)
{
    if (duty_percent > 100U) {
        duty_percent = 100U;
    }

    s_duty_percent = duty_percent;
    tim2_apply_duty();
}
