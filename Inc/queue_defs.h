#ifndef QUEUE_DEFS_H
#define QUEUE_DEFS_H

#include <stdint.h>
#include "config.h"

/* Task_Sensor -> Task_Judge */
typedef struct {
    uint16_t adc_raw;         // EMA 평활화된 신호
    uint16_t adc_instant;     // 필터 전 원본 (진단용: 센서 무반응 vs 필터 뭉갬 구분)
    uint16_t baseline;        // 적응형 기준선 (평상시 공기 수준)
    int16_t  delta;           // 신호 - 기준선. ★ 판정은 이 값으로 한다
    float    rate_of_rise;    // delta의 변화율 (counts/sec), FR7 MLP 입력용
    int16_t  temp_c_x10;      // TMP102, 0.1도 단위 정수 (FR6, 로그용)
    uint8_t  baseline_ready;  // 기준선 학습 유예 종료 여부. 0이면 경보 억제
} SensorData_t;

/* Task_Judge -> Task_Output */
typedef struct {
    uint16_t   adc_raw;
    uint16_t   adc_instant;   // 진단용 원본값
    uint16_t   baseline;
    int16_t    delta;
    RiskLevel  risk;
    uint8_t    alarm_deterministic;  // FR2: 임계값 초과 여부
    uint8_t    alarm_mlp;            // FR7: MLP 조기경보 여부
    SystemMode mode;
    /* 아래 둘은 판정에는 쓰이지 않고 UART 로그 전용이다 (FR6/FR8).
     * Task_Output이 센서 큐를 따로 볼 필요가 없도록 여기 실어 보낸다. */
    int16_t    temp_c_x10;
    float      rate_of_rise;
} JudgeResult_t;

#endif
