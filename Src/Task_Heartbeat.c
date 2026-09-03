/*-----------------------------------------------------------
 * Task_Heartbeat — 시스템 생존 신호 (CLAUDE.md 7장 1단계)
 *
 * 1단계 목표는 "커널이 링크되고 스케줄러가 실제로 돈다"를 증명하는 것.
 * LED 점멸만으로는 for 루프 딜레이와 구분되지 않으므로 UART 카운터를 함께 출력한다.
 * vTaskDelay가 정확한 주기로 깨어난다는 것 = SysTick + 커널 tick이 살아있다는 뜻이다.
 *
 * 1단계 임시 코드가 아니라 이후에도 heartbeat 용도로 계속 유지한다.
 *-----------------------------------------------------------*/

#include "stm32l476xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "config.h"
#include "tasks_app.h"

/* 로그를 찍을 주기인지 판정. 진단 모드에서는 매 주기 찍는다. */
#if HEARTBEAT_PIN_DEBUG
  #define HB_SHOULD_LOG(c)   (1)
#else
  #define HB_SHOULD_LOG(c)   ((((c) % HEARTBEAT_LOG_EVERY_N)) == 0U)
#endif

void Task_Heartbeat(void *pvParameters)
{
    (void)pvParameters;

    uint16_t count = 0;

#if HEARTBEAT_LED_TEST
    /* 3단계 임시: 부팅 직후 LED 6개를 순차 점등해 배선을 확인한다.
     * 스케줄러가 이미 돌고 있으므로 vTaskDelay가 정상 동작한다. */
    LED_SelfTest();

    RiskLevel  test_risk = RISK_SAFE;
    SystemMode test_mode = MODE_NORMAL;
    LED_SetRisk(test_risk);
    LED_SetMode(test_mode);
#endif

    /* vTaskDelayUntil 기준 시각.
     * vTaskDelay는 "깨어난 뒤 N ms"라 루프 본문 실행시간만큼 주기가 밀리지만,
     * vTaskDelayUntil은 절대 시각 기준이라 드리프트가 누적되지 않는다.
     * 주기 정확도가 검증 포인트이므로 여기서는 후자를 쓴다. */
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        /* PA5 토글.
         * ODR을 |= / &= 로 직접 건드리면 read-modify-write라 인터럽트와 경합할 수 있다.
         * 쓰기는 원자적인 BSRR(set) / BRR(reset)로 하고, 현재 상태 판단만 ODR을 읽는다.
         * (RM0351 8.5.5 BSRR / 8.5.6 BRR) */
        if (GPIOA->ODR & (1U << LD2_PIN)) {
            GPIOA->BRR  = (1U << LD2_PIN);   /* 소등 */
        } else {
            GPIOA->BSRR = (1U << LD2_PIN);   /* 점등 */
        }

        /* 스케줄러 동작의 실제 증거. LED는 매 주기 토글하되 로그는 솎아낸다.
         * 진단 모드에서는 버튼 상태 변화를 놓치면 안 되므로 매 주기 출력한다. */
        if (HB_SHOULD_LOG(count)) {
            UART_SendString("[HB] ");
            UART_SendNumber(count);          /* uint16_t라 65535에서 wrap. 무해. */

#if HEARTBEAT_PIN_DEBUG
            /* 버튼 핀 raw 입력. EXTI를 거치지 않고 IDR을 직접 읽으므로
             * 배선 문제(값이 안 변함)와 EXTI 설정 문제(값은 변하는데 인터럽트가 안 옴)를 구분할 수 있다. */
            UART_SendString("  D4=");
            UART_SendChar((GPIOB->IDR & (1U << BTN_MODE_PIN))     ? '1' : '0');
            UART_SendString(" D9=");
            UART_SendChar((GPIOC->IDR & (1U << BTN_MUTE_PIN))     ? '1' : '0');
            UART_SendString(" B1=");
            UART_SendChar((GPIOC->IDR & (1U << BTN_SELFTEST_PIN)) ? '1' : '0');
#endif

            UART_SendString("\r\n");
        }

#if HEARTBEAT_BEEP_TEST
        /* 2단계 임시: N회마다 짧게 삑. 부저 회로 + TIM2 PWM 확인용.
         * vTaskDelayUntil은 절대 시각 기준이라 이 지연만큼 다음 주기가 밀리지 않고 자동 보정된다. */
        if ((count % HEARTBEAT_BEEP_EVERY_N) == 0U) {
            UART_SendString("[HB] beep\r\n");
            TIM2_SetFrequency(BUZZER_FREQ_SELFTEST_HZ);
            TIM2_SetDuty(50);
            vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_BEEP_MS));
            TIM2_SetDuty(0);
        }
#endif

#if HEARTBEAT_LED_TEST
        /* 3단계 임시: 5초마다 위험도 LED를 한 칸씩 돌리고, 한 바퀴 돌면 모드를 바꾼다.
         * LED_SetRisk / LED_SetMode 두 API가 실제로 동작하는지 눈으로 확인하는 용도. */
        if ((count % HEARTBEAT_BEEP_EVERY_N) == 0U) {
            if (test_risk == RISK_DANGER) {
                test_risk = RISK_SAFE;
                test_mode = (test_mode == MODE_NORMAL) ? MODE_SLEEP : MODE_NORMAL;
                LED_SetMode(test_mode);
                UART_SendString("[HB] mode -> ");
                UART_SendNumber((uint16_t)test_mode);
                UART_SendString("\r\n");
            } else {
                test_risk = (RiskLevel)((int)test_risk + 1);
            }
            LED_SetRisk(test_risk);
            UART_SendString("[HB] risk -> ");
            UART_SendNumber((uint16_t)test_risk);
            UART_SendString("\r\n");
        }
#endif

        count++;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(HEARTBEAT_PERIOD_MS));
    }
}
