#ifndef CONFIG_H
#define CONFIG_H

#include "stm32l476xx.h"

/* ===== 센서/판정 파라미터 ===== */
#define SENSOR_PERIOD_NORMAL_MS   1000
#define SENSOR_PERIOD_SLEEP_MS    300

#define CO_THRESHOLD_NORMAL       500
#define CO_THRESHOLD_SLEEP        350   // NORMAL의 70%

#define MOVING_AVG_WINDOW          4    // 이동평균 샘플 개수

/* ===== 부저 PWM (TIM2_CH2 / PA1) ===== */
/* TIM2 입력 클럭 = APB1 타이머 클럭. 현재 SYSCLK = MSI 4MHz, APB1 prescaler = 1 이므로 4MHz.
 * ★ 클럭을 올리면 이 값도 반드시 수정할 것 (CLAUDE.md 1.1: FreeRTOSConfig, USART BRR와 함께) */
#define TIM2_CLK_HZ            4000000UL
/* PWM 내부 카운터 틱. PSC로 TIM2_CLK_HZ를 이 값까지 분주한다.
 * 1MHz면 주파수 = 1e6/(ARR+1), 2~5kHz 대역에서 duty 분해능 200~500스텝으로 충분. */
#define BUZZER_TICK_HZ        1000000UL
#define BUZZER_FREQ_NORMAL_HZ    2000   // 일반모드 경보음
#define BUZZER_FREQ_SLEEP_HZ     2731   // 취침모드: GEC09D 공진주파수 부근 = 최대 음압
#define BUZZER_FREQ_SELFTEST_HZ  2000   // 자가진단 / 부팅 확인음
#define BUZZER_ACK_MS              60   // 버튼 입력 확인음 길이

/* ===== 버튼 ===== */
/* 상태 변화 후 이 시간 동안의 추가 엣지는 채터링으로 보고 버린다.
 * 접점 바운스는 보통 1~10ms라 50ms면 충분히 덮는다. */
#define BUTTON_DEBOUNCE_MS         50

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