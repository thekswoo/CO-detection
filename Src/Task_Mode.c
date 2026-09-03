/*-----------------------------------------------------------
 * Task_Mode — 버튼 이벤트 처리 (CLAUDE.md 7장 4단계)
 *
 *   버튼1 (PB5,  D4) : 모드 토글 (일반 <-> 취침)   FR4/FR5/FR9
 *   버튼2 (PC7,  D9) : 경보 음소거 토글            FR4/FR11
 *   B1    (PC13)     : 자가진단                    FR12
 *   RESET (내장)     : 비상 리셋 (하드웨어)        FR4
 *
 * ISR(button.c)이 큐로 이벤트를 넘기고, 여기서 실제 상태를 바꾼다.
 * 인터럽트 컨텍스트에서는 UART/부저/vTaskDelay를 쓸 수 없으므로
 * 무거운 동작은 전부 이 태스크에서 처리한다.
 *-----------------------------------------------------------*/

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "config.h"
#include "tasks_app.h"

/* 시스템 전역 상태. Task_Mode만 write하고 나머지 태스크는 read한다.
 * Cortex-M4에서 정렬된 32비트 접근은 원자적이라 별도 락 없이 volatile로 충분하다. */
volatile SystemMode g_mode  = MODE_NORMAL;
volatile uint8_t    g_muted = 0;

/* 자가진단 시퀀스가 도는 동안 1. Task_Output이 이걸 보고 LED/부저를 건드리지 않는다.
 * 없으면 1초마다 오는 판정 결과가 순차 점등을 덮어써서 시퀀스가 보이지 않는다. */
volatile uint8_t    g_selftest_active = 0;

extern QueueHandle_t xQueueButton;

/* 버튼이 먹었다는 촉각 피드백. 짧게 한 번. */
static void ack_beep(uint16_t freq_hz)
{
    TIM2_SetFrequency(freq_hz);
    TIM2_SetDuty(50);
    vTaskDelay(pdMS_TO_TICKS(BUZZER_ACK_MS));
    TIM2_SetDuty(0);
}

/* FR12 자가진단: LED 6개 순차 점등 + 부저 2음.
 * 끝나면 LED를 현재 상태로 되돌려야 사용자가 헷갈리지 않는다. */
static void run_selftest(void)
{
    UART_SendString("[SELFTEST] start\r\n");

    g_selftest_active = 1;                /* Task_Output의 LED 갱신을 잠시 막는다 */

    LED_SelfTest();                       /* led.c 재사용 (3단계에서 구현) */

    ack_beep(BUZZER_FREQ_NORMAL_HZ);
    vTaskDelay(pdMS_TO_TICKS(80));
    ack_beep(BUZZER_FREQ_SLEEP_HZ);

    /* 모드 표시를 먼저 복구한 뒤 플래그를 내린다.
     * 순서가 반대면 그 사이에 들어온 판정 결과와 겹쳐 잠깐 깜빡인다.
     * 위험도 LED는 다음 판정 결과가 오면 Task_Output이 곧바로 덮어쓴다. */
    LED_SetMode(g_mode);
    g_selftest_active = 0;

    UART_SendString("[SELFTEST] done\r\n");
}

void Task_Mode(void *pvParameters)
{
    (void)pvParameters;

    ButtonEvent evt;

    /* 부팅 자가진단 1회. 하드웨어가 다 살아있는지 눈/귀로 확인하고 시작한다. */
    run_selftest();

    for (;;) {
        if (xQueueReceive(xQueueButton, &evt, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (evt) {
            case BTN_EVT_MODE:
                g_mode = (g_mode == MODE_NORMAL) ? MODE_SLEEP : MODE_NORMAL;
                LED_SetMode(g_mode);
                UART_SendString((g_mode == MODE_SLEEP)
                                ? "[MODE] sleep\r\n" : "[MODE] normal\r\n");
                /* 취침모드는 더 높은 음으로 구분되게 알린다. */
                ack_beep((g_mode == MODE_SLEEP)
                         ? BUZZER_FREQ_SLEEP_HZ : BUZZER_FREQ_NORMAL_HZ);
                break;

            case BTN_EVT_MUTE:
                g_muted = !g_muted;
                UART_SendString(g_muted ? "[MUTE] on\r\n" : "[MUTE] off\r\n");
                /* 음소거를 켜는 순간에 소리를 내면 앞뒤가 안 맞으므로,
                 * 해제할 때만 확인음을 낸다. FR11: 판정/LED는 그대로 유지된다. */
                if (!g_muted) {
                    ack_beep(BUZZER_FREQ_NORMAL_HZ);
                }
                break;

            case BTN_EVT_SELFTEST:
                run_selftest();
                break;

            default:
                break;
        }
    }
}
