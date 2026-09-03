/*-----------------------------------------------------------
 * Task_Output — LED / 부저 / UART 출력 (FR3, FR5, FR8, FR9, FR11)
 *
 * Task_Judge가 큐로 넘긴 판정 결과를 사람이 인지할 수 있는 형태로 바꾼다.
 * 판정은 하지 않는다. 여기서 하는 건 표현뿐이다.
 *
 * LED 제어는 led.c의 API를 쓴다. 이전 스켈레톤은 GPIOB/GPIOC를 직접 만지는
 * 코드를 자체적으로 갖고 있었는데, led.c와 핀 정의가 이중으로 존재해
 * 한쪽만 고치면 어긋나는 구조였다.
 *-----------------------------------------------------------*/

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "config.h"
#include "queue_defs.h"
#include "tasks_app.h"

extern QueueHandle_t xQueueJudgeToOutput;

extern volatile uint8_t g_muted;            /* FR11 */
extern volatile uint8_t g_selftest_active;  /* 자가진단 중 LED 덮어쓰기 방지 */

/* 위험도 + 모드에 따른 경보음.
 * FR5: 취침모드에서는 공진주파수 부근으로 올려 음압을 최대화한다. */
static void update_buzzer(const JudgeResult_t *r)
{
    /* FR11: 음소거는 부저만 끈다. 판정도 LED도 그대로 유지된다. */
    if (g_muted || r->risk == RISK_SAFE) {
        TIM2_SetDuty(0);
        return;
    }

    uint16_t freq = (r->mode == MODE_SLEEP)
                    ? BUZZER_FREQ_SLEEP_HZ
                    : BUZZER_FREQ_NORMAL_HZ;

    TIM2_SetFrequency(freq);

    /* RISK_DANGER는 연속음, RISK_WARN(MLP 조기경보)은 약하게 울려 구분한다.
     * 조기경보 단계에서 위험 단계와 같은 소리를 내면 경보 피로가 생긴다. */
    TIM2_SetDuty((r->risk == RISK_DANGER) ? 50 : 15);
}

/* UART_SendNumber가 uint16_t만 받으므로 음수용 래퍼를 둔다. delta는 음수가 될 수 있다. */
static void send_signed(int16_t v)
{
    if (v < 0) {
        UART_SendChar('-');
        v = (int16_t)(-v);
    }
    UART_SendNumber((uint16_t)v);
}

/* 부팅 이후 관측된 delta 최고값. 임계값 튜닝용.
 * 흘러가는 로그에서 피크를 눈으로 잡기 어려워 펌웨어가 대신 기록한다.
 * RESET 버튼을 누르면 초기화된다. */
static int16_t s_peak_delta = 0;

static void log_result(const JudgeResult_t *r)
{
    /* snprintf 대신 기존 UART 헬퍼를 쓴다.
     * configUSE_NEWLIB_REENTRANT가 0이라 여러 태스크에서 newlib을 호출하면
     * 재진입 문제가 생길 수 있고, 스택도 훨씬 덜 먹는다. */
    UART_SendString("[CO] raw=");
    UART_SendNumber(r->adc_instant);   /* 필터 전. 센서가 실제로 반응하는지 확인용 */
    UART_SendString(" adc=");
    UART_SendNumber(r->adc_raw);

    /* 튜닝용. 평상시 delta 변동 폭을 보고 CO_DELTA_* 값을 정한다. */
    UART_SendString(" base=");
    UART_SendNumber(r->baseline);
    UART_SendString(" delta=");
    send_signed(r->delta);

    /* 워밍업 구간은 제외한다. 히터 켜질 때의 과도 상승(400대)이 기록되면
     * 이후 자극 응답 피크와 구분이 안 돼 임계값 근거로 못 쓴다. */
    if (xTaskGetTickCount() >= pdMS_TO_TICKS(BASELINE_WARMUP_MS)
        && r->delta > s_peak_delta) {
        s_peak_delta = r->delta;
    }
    UART_SendString(" peak=");
    send_signed(s_peak_delta);

    UART_SendString(" risk=");
    UART_SendNumber((uint16_t)r->risk);

    UART_SendString(" mode=");
    UART_SendString((r->mode == MODE_SLEEP) ? "sleep" : "normal");

    UART_SendString(" mute=");
    UART_SendNumber((uint16_t)g_muted);

    /* FR6(TMP102 온도)는 범위에서 제외했다. 스텁이 항상 0을 반환하는데
     * 로그에 temp=0을 찍으면 고장난 센서처럼 보인다.
     * queue_defs.h의 temp_c_x10 필드와 tmp102.c 스텁은 남겨둔다 —
     * 나중에 센서를 붙이면 이 두 줄만 되살리면 된다. */

    UART_SendString(" mlp=");
    UART_SendNumber((uint16_t)r->alarm_mlp);

    UART_SendString("\r\n");
}

void Task_Output(void *pvParameters)
{
    (void)pvParameters;

    JudgeResult_t result;

    for (;;) {
        if (xQueueReceive(xQueueJudgeToOutput, &result, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        /* 자가진단 중에는 LED/부저를 건드리지 않는다.
         * Task_Mode가 순차 점등 시퀀스를 도는 중에 여기서 덮어쓰면
         * 사용자가 어느 LED가 켜진 건지 판단할 수 없게 된다.
         * 단, 판정 결과 로그는 계속 남긴다 (FR11의 "판정은 절대 정지 금지"와 같은 취지). */
        if (!g_selftest_active) {
            LED_SetRisk(result.risk);     /* FR3 */
            LED_SetMode(result.mode);     /* FR9 */
            update_buzzer(&result);       /* FR2, FR5, FR11 */
        }

        log_result(&result);              /* FR8 */
    }
}
