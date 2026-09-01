/*-----------------------------------------------------------
 * LED 출력 드라이버 (CLAUDE.md 7장 3단계)
 *
 *   위험도 LED (GPIOB) : PB0 초록 / PB1 노랑 / PB2 빨강
 *                        GPIO --[330R]-- 애노드, 캐소드 -- GND
 *   모드 RGB  (GPIOC) : PC0 R / PC1 G / PC2 B, 공통음극
 *                        GPIO --[330R]-- 색상 애노드, 공통 -- GND
 *
 * 두 회로 모두 GPIO High = 점등이다.
 * 참조: RM0351 8장 (GPIO)
 *-----------------------------------------------------------*/

#include "stm32l476xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "config.h"
#include "tasks_app.h"

void LED_Init(void)
{
    RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN);

    /* --- 위험도 LED: PB0/PB1/PB2 범용 출력(01) ---
     * GPIOB의 MODER 리셋값은 0이 아니다(PB3/PB4가 JTAG용 AF로 잡혀 있음).
     * 리셋값에 의존하지 말고 clear-before-set으로 강제한다 (CLAUDE.md 0.5). */
    GPIOB->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1 | GPIO_MODER_MODE2);
    GPIOB->MODER |=  (GPIO_MODER_MODE0_0 | GPIO_MODER_MODE1_0 | GPIO_MODER_MODE2_0);

    GPIOB->OTYPER &= ~LED_RISK_MASK;                    /* push-pull */
    GPIOB->PUPDR  &= ~(GPIO_PUPDR_PUPD0 | GPIO_PUPDR_PUPD1 | GPIO_PUPDR_PUPD2);  /* 풀업/풀다운 없음 */

    /* --- 모드 RGB: PC0/PC1/PC2 범용 출력(01) --- */
    GPIOC->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1 | GPIO_MODER_MODE2);
    GPIOC->MODER |=  (GPIO_MODER_MODE0_0 | GPIO_MODER_MODE1_0 | GPIO_MODER_MODE2_0);

    GPIOC->OTYPER &= ~RGB_MASK;
    GPIOC->PUPDR  &= ~(GPIO_PUPDR_PUPD0 | GPIO_PUPDR_PUPD1 | GPIO_PUPDR_PUPD2);

    /* OSPEEDR는 리셋값(low speed) 그대로. LED 점멸에 슬루레이트가 필요 없다. */

    LED_AllOff();
}

void LED_AllOff(void)
{
    /* BRR은 쓰기 한 번으로 지정 비트만 0으로 만든다(read-modify-write 없음). */
    GPIOB->BRR = LED_RISK_MASK;
    GPIOC->BRR = RGB_MASK;
}

/* 위험도 3단계를 배타적으로 표시한다. 항상 한 색만 켜진다. */
void LED_SetRisk(RiskLevel risk)
{
    GPIOB->BRR = LED_RISK_MASK;          /* 먼저 3색 모두 소등 */

    switch (risk) {
        case RISK_SAFE:   GPIOB->BSRR = (1U << LED_GREEN_PIN);  break;
        case RISK_WARN:   GPIOB->BSRR = (1U << LED_YELLOW_PIN); break;
        case RISK_DANGER: GPIOB->BSRR = (1U << LED_RED_PIN);    break;
        default:          break;         /* 알 수 없는 값이면 전부 소등 상태 유지 */
    }
}

/* 동작 모드를 RGB LED로 표시한다 (FR9). 일반=초록, 취침=파랑. */
void LED_SetMode(SystemMode mode)
{
    GPIOC->BRR = RGB_MASK;

    if (mode == MODE_SLEEP) {
        GPIOC->BSRR = (1U << RGB_B_PIN);
    } else {
        GPIOC->BSRR = (1U << RGB_G_PIN);
    }
}

/* FR12 자가진단: LED를 하나씩 순차 점등해 배선/소자 정상 여부를 눈으로 확인한다.
 * vTaskDelay를 쓰므로 스케줄러가 돈 뒤(태스크 컨텍스트)에만 호출할 것. */
void LED_SelfTest(void)
{
    static const uint32_t risk_pins[3] = {
        (1U << LED_GREEN_PIN), (1U << LED_YELLOW_PIN), (1U << LED_RED_PIN)
    };
    static const uint32_t rgb_pins[3] = {
        (1U << RGB_R_PIN), (1U << RGB_G_PIN), (1U << RGB_B_PIN)
    };

    LED_AllOff();

    UART_SendString("[TEST] risk LED: green -> yellow -> red\r\n");
    for (int i = 0; i < 3; i++) {
        GPIOB->BSRR = risk_pins[i];
        vTaskDelay(pdMS_TO_TICKS(LED_SELFTEST_STEP_MS));
        GPIOB->BRR  = risk_pins[i];
    }

    UART_SendString("[TEST] mode RGB: R -> G -> B\r\n");
    for (int i = 0; i < 3; i++) {
        GPIOC->BSRR = rgb_pins[i];
        vTaskDelay(pdMS_TO_TICKS(LED_SELFTEST_STEP_MS));
        GPIOC->BRR  = rgb_pins[i];
    }

    /* 마지막으로 전부 동시 점등 — 배선 누락이 있으면 여기서 바로 티가 난다. */
    UART_SendString("[TEST] all on\r\n");
    GPIOB->BSRR = LED_RISK_MASK;
    GPIOC->BSRR = RGB_MASK;
    vTaskDelay(pdMS_TO_TICKS(LED_SELFTEST_STEP_MS * 2));

    LED_AllOff();
    UART_SendString("[TEST] led selftest done\r\n");
}
