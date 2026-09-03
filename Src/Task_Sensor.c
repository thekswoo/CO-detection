/*-----------------------------------------------------------
 * Task_Sensor — MQ-7 ADC 측정, 필터링, 기준선 학습 (FR1, FR6)
 *
 * 이 태스크의 출력은 세 가지다.
 *   signal   : EMA로 평활화한 현재 신호
 *   baseline : 평상시 공기 수준을 천천히 학습한 기준선
 *   delta    : signal - baseline  <- Task_Judge가 실제로 보는 값
 *
 * 왜 절대값이 아니라 delta인가:
 *   MQ-7을 히터 사이클링 없이 상시 5V로 구동하고 있어 절대 ADC 값이
 *   매 부팅마다 다른 지점으로 수렴하고 수십 초 단위로 드리프트한다.
 *   고정 임계값(예: 500)은 이 조건에서 성립하지 않는다.
 *   기준선 대비 상승량으로 보면 센서가 어디에 정착하든 평상시가 0이 된다.
 *
 * 왜 이동평균이 아니라 EMA인가:
 *   이동평균은 창이 "샘플 개수"라서, 모드에 따라 주기가 1000ms <-> 300ms로
 *   바뀌면 실제 시간 창이 3.3배 달라진다. 같은 CO 상승인데도 모드마다 다른
 *   기울기가 나와 MLP 입력이 일관되지 않는다.
 *   EMA는 alpha = dt/(tau+dt)로 dt에 따라 자동 보정되므로 시정수가 고정된다.
 *-----------------------------------------------------------*/

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "config.h"
#include "queue_defs.h"
#include "tasks_app.h"

extern QueueHandle_t xQueueSensorToJudge;
extern volatile SystemMode g_mode;

void Task_Sensor(void *pvParameters)
{
    (void)pvParameters;

    SensorData_t data;

    float signal     = 0.0f;
    float baseline   = 0.0f;
    float prev_delta = 0.0f;
    uint8_t first_run = 1;
    TickType_t freeze_since = 0;      /* 기준선 동결이 시작된 시각. 0 = 동결 아님 */

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        uint32_t period_ms = (g_mode == MODE_SLEEP)
                             ? SENSOR_PERIOD_SLEEP_MS
                             : SENSOR_PERIOD_NORMAL_MS;
        float dt_s = (float)period_ms / 1000.0f;

        uint16_t raw = ADC_Read();     /* ADC 하드웨어 오버샘플링 적용된 값 */

        if (first_run) {
            /* 첫 샘플로 신호와 기준선을 동시에 초기화한다.
             * 0에서 시작하면 기준선이 실제 수준까지 올라오는 데 수 분이 걸려
             * 그동안 delta가 거대해진다. */
            signal   = (float)raw;
            baseline = (float)raw;
            first_run = 0;
        } else {
            float alpha_sig = dt_s / (SIGNAL_TAU_S + dt_s);
            signal += alpha_sig * ((float)raw - signal);
        }

        float delta = signal - baseline;

        /* 기준선 학습.
         * delta가 크다는 건 뭔가 올라오고 있다는 뜻이므로 학습을 멈춘다.
         * 멈추지 않으면 서서히 올라오는 진짜 누출이 기준선에 흡수돼 경보가 안 울린다.
         *
         * 단 신호가 기준선보다 낮아지는 방향(signal < baseline)은 항상 추종한다.
         * 안 그러면 한 번 기준선이 높게 잡힌 뒤 영영 안 내려와 오경보가 고착된다. */
        uint8_t freeze = (delta >= (float)BASELINE_FREEZE_DELTA) && (signal >= baseline);

        /* 동결 시간 제한.
         * 센서가 예열/온도 변화로 서서히 위로 드리프트하면 동결이 풀리지 않아
         * 가짜 경보가 고착된다. 일정 시간이 지나면 드리프트로 간주하고 학습을 재개한다. */
        TickType_t now = xTaskGetTickCount();
        if (freeze) {
            if (freeze_since == 0) {
                freeze_since = now;
            } else if ((now - freeze_since) > pdMS_TO_TICKS(BASELINE_FREEZE_MAX_MS)) {
                freeze = 0;
                UART_SendString("[BASE] freeze timeout -> relearn\r\n");
                freeze_since = now;
            }
        } else {
            freeze_since = 0;
        }

        if (now < pdMS_TO_TICKS(BASELINE_WARMUP_MS)) {
            /* 워밍업: 기준선을 신호에 하드 고정한다.
             * 히터 과도 구간을 EMA로 학습시키면 기준선이 수백 카운트 지점에
             * 물려버려, 이후 신호가 정상으로 내려와도 delta가 크게 음수로
             * 남는다. 워밍업이 끝나는 순간 delta=0에서 깨끗하게 출발시킨다. */
            baseline = signal;
            freeze_since = 0;
        } else if (!freeze) {
            /* 상승은 느리게, 하강은 빠르게 (비대칭).
             * 기준선이 높게 남는 쪽이 경보를 죽이므로 위험 방향이다. */
            float tau = (signal < baseline) ? BASELINE_TAU_FALL_S : BASELINE_TAU_S;
            float alpha_base = dt_s / (tau + dt_s);
            baseline += alpha_base * (signal - baseline);
        }

        data.adc_raw     = (uint16_t)signal;
        data.adc_instant = raw;                    /* 필터 전 원본 (진단용) */
        data.baseline    = (uint16_t)baseline;
        data.delta       = (int16_t)delta;
        data.temp_c_x10  = TMP102_ReadTempX10();   /* FR6, 로그 전용 */

        /* FR7 입력. delta 기준이라 기준선 드리프트가 기울기에 섞이지 않는다. */
        data.rate_of_rise = (delta - prev_delta) / dt_s;
        prev_delta = delta;

        /* 부팅 직후에는 기준선이 아직 실제 공기 수준을 대표하지 못한다.
         * 이 구간에서 경보를 내면 켤 때마다 오경보가 난다. */
        data.baseline_ready = (now >= pdMS_TO_TICKS(BASELINE_WARMUP_MS)) ? 1U : 0U;

        xQueueSend(xQueueSensorToJudge, &data, 0);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(period_ms));
    }
}
