#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "config.h"
#include "queue_defs.h"

extern QueueHandle_t xQueueSensorToJudge;
extern QueueHandle_t xQueueJudgeToOutput;
extern volatile SystemMode g_mode;

float mlp_infer(float co_val, float rate_of_rise);  // FR7, 나중에 연결

void Task_Judge(void *pvParameters) {
    SensorData_t sensor_data;
    JudgeResult_t result;

    for (;;) {
        xQueueReceive(xQueueSensorToJudge, &sensor_data, portMAX_DELAY);

        SystemMode mode = g_mode;
        uint16_t threshold = (mode == MODE_SLEEP)
                              ? CO_THRESHOLD_SLEEP
                              : CO_THRESHOLD_NORMAL;

        result.adc_raw = sensor_data.adc_raw;
        result.mode = mode;

        // FR2: 결정론적 임계값 판정 (항상 동작, MLP 유무와 무관)
        result.alarm_deterministic = (sensor_data.adc_raw > threshold) ? 1 : 0;

        // FR7: MLP 조기경보 (스코프 진행 중이면 0으로 두고 나중에 활성화)
        float risk_prob = mlp_infer((float)sensor_data.adc_raw, sensor_data.rate_of_rise);
        result.alarm_mlp = (risk_prob > 0.5f) ? 1 : 0;

        // FR3: 3단계 위험도 매핑
        if (result.alarm_deterministic) {
            result.risk = RISK_DANGER;
        } else if (result.alarm_mlp) {
            result.risk = RISK_WARN;
        } else {
            result.risk = RISK_SAFE;
        }

        xQueueSend(xQueueJudgeToOutput, &result, 0);
    }
}
