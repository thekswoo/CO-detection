#ifndef CONFIG_H
#define CONFIG_H

#include "stm32l476xx.h"

/* ===== 센서/판정 파라미터 ===== */
#define SENSOR_PERIOD_NORMAL_MS   1000
#define SENSOR_PERIOD_SLEEP_MS    300

#define CO_THRESHOLD_NORMAL       500
#define CO_THRESHOLD_SLEEP        350   // NORMAL의 70%

#define MOVING_AVG_WINDOW          4    // 이동평균 샘플 개수

/* ===== 시스템 모드 ===== */
typedef enum {
    MODE_NORMAL = 0,
    MODE_SLEEP  = 1
} SystemMode;

/* ===== 위험도 레벨 (FR3) ===== */
typedef enum {
    RISK_SAFE   = 0,   // 초록
    RISK_WARN   = 1,   // 노랑 (FR7 MLP 조기경보 포함)
    RISK_DANGER = 2    // 빨강 (FR2 즉시경보)
} RiskLevel;

#endif