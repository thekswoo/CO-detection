#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "config.h"
#include "queue_defs.h"

extern QueueHandle_t xQueueSensorToJudge;
extern volatile SystemMode g_mode;

uint16_t ADC_Read(void);          // 기존 구현 재사용
int16_t  TMP102_ReadTempX10(void); // I2C, 미구현이면 0 반환하도록 스텁

static uint16_t history[MOVING_AVG_WINDOW] = {0};
static uint8_t  hist_idx = 0;
static uint8_t  hist_filled = 0;

static uint16_t moving_average(uint16_t new_val) {
    history[hist_idx] = new_val;
    hist_idx = (hist_idx + 1) % MOVING_AVG_WINDOW;
    if (hist_idx == 0) hist_filled = 1;

    uint32_t sum = 0;
    uint8_t count = hist_filled ? MOVING_AVG_WINDOW : hist_idx;
    if (count == 0) return new_val;

    for (uint8_t i = 0; i < count; i++) sum += history[i];
    return (uint16_t)(sum / count);
}

void Task_Sensor(void *pvParameters) {
    static uint16_t prev_avg = 0;
    static uint8_t  first_run = 1;
    SensorData_t data;

    for (;;) {
        uint16_t raw = ADC_Read();
        uint16_t avg = moving_average(raw);

        data.adc_raw = avg;
        data.temp_c_x10 = TMP102_ReadTempX10();

        // FR7 입력용 상승률: (현재-이전) / 주기(초)
        if (first_run) {
            data.rate_of_rise = 0.0f;
            first_run = 0;
        } else {
            float dt_s = (g_mode == MODE_SLEEP)
                         ? (SENSOR_PERIOD_SLEEP_MS / 1000.0f)
                         : (SENSOR_PERIOD_NORMAL_MS / 1000.0f);
            data.rate_of_rise = ((float)avg - (float)prev_avg) / dt_s;
        }
        prev_avg = avg;

        xQueueSend(xQueueSensorToJudge, &data, 0);

        uint32_t period = (g_mode == MODE_SLEEP)
                           ? SENSOR_PERIOD_SLEEP_MS
                           : SENSOR_PERIOD_NORMAL_MS;
        vTaskDelay(pdMS_TO_TICKS(period));
    }
}
