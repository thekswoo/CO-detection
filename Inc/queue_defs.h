#ifndef QUEUE_DEFS_H
#define QUEUE_DEFS_H

#include <stdint.h>
#include "config.h"

/* Task_Sensor -> Task_Judge */
typedef struct {
    uint16_t adc_raw;
    float    rate_of_rise;   // FR7 MLP 입력용
    int16_t  temp_c_x10;     // TMP102, 0.1도 단위 정수 (FR6, 로그용)
} SensorData_t;

/* Task_Judge -> Task_Output */
typedef struct {
    uint16_t   adc_raw;
    RiskLevel  risk;
    uint8_t    alarm_deterministic;  // FR2: 임계값 초과 여부
    uint8_t    alarm_mlp;            // FR7: MLP 조기경보 여부
    SystemMode mode;
} JudgeResult_t;

#endif
