/*-----------------------------------------------------------
 * Task_Judge — 위험도 판정 (FR2, FR5, FR7)
 *
 * 이중 판단 구조:
 *   1) 결정론적 임계값 — 항상 동작. MLP가 어떻게 되든 독립적으로 판정한다.
 *   2) MLP 조기경보    — CO 농도와 상승 기울기로 예방적 경고 (9단계에서 활성)
 * 결정론적 위험 판정이 MLP보다 우선한다 (CLAUDE.md 4.4).
 *
 * 판정 입력은 절대 ADC 값이 아니라 Task_Sensor가 계산한 delta(기준선 대비
 * 상승량)다. 센서가 매번 다른 지점으로 수렴하고 드리프트하기 때문이다.
 *-----------------------------------------------------------*/

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "config.h"
#include "queue_defs.h"
#include "tasks_app.h"

extern QueueHandle_t xQueueSensorToJudge;
extern QueueHandle_t xQueueJudgeToOutput;
extern volatile SystemMode g_mode;

void Task_Judge(void *pvParameters)
{
    (void)pvParameters;

    SensorData_t  sensor_data;
    JudgeResult_t result;

    for (;;) {
        if (xQueueReceive(xQueueSensorToJudge, &sensor_data, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        SystemMode mode = g_mode;

        /* FR5: 취침모드에서는 같은 상승량이라도 더 민감하게 판정한다. */
        int16_t warn_th   = (mode == MODE_SLEEP)
                            ? CO_DELTA_WARN_SLEEP   : CO_DELTA_WARN_NORMAL;
        int16_t danger_th = (mode == MODE_SLEEP)
                            ? CO_DELTA_DANGER_SLEEP : CO_DELTA_DANGER_NORMAL;

        result.adc_raw      = sensor_data.adc_raw;
        result.adc_instant  = sensor_data.adc_instant;
        result.baseline     = sensor_data.baseline;
        result.delta        = sensor_data.delta;
        result.mode         = mode;
        result.temp_c_x10   = sensor_data.temp_c_x10;   /* 로그 전용 (FR6) */
        result.rate_of_rise = sensor_data.rate_of_rise;

        if (!sensor_data.baseline_ready) {
            /* 기준선 학습 유예 구간. 판정과 로그는 계속하되 경보만 내지 않는다. */
            result.alarm_deterministic = 0;
            result.alarm_mlp           = 0;
            result.risk                = RISK_SAFE;
        } else {
            /* FR2: 결정론적 판정. MLP 유무와 무관하게 항상 동작한다. */
            result.alarm_deterministic = (sensor_data.delta >= danger_th) ? 1 : 0;

            /* FR7: MLP 조기경보. 9단계 전까지 mlp_infer()는 0.0을 반환한다. */
            float risk_prob = mlp_infer((float)sensor_data.delta,
                                        sensor_data.rate_of_rise);
            result.alarm_mlp = (risk_prob > 0.5f) ? 1 : 0;

            /* FR3: 3단계 위험도.
             * 경고는 "임계값 경고 수준 도달" 또는 "MLP 조기경보" 둘 중 하나로 뜬다. */
            if (result.alarm_deterministic) {
                result.risk = RISK_DANGER;
            } else if (sensor_data.delta >= warn_th || result.alarm_mlp) {
                result.risk = RISK_WARN;
            } else {
                result.risk = RISK_SAFE;
            }
        }

        xQueueSend(xQueueJudgeToOutput, &result, 0);
    }
}
