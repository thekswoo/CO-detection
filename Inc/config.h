#ifndef CONFIG_H
#define CONFIG_H

#include "stm32l476xx.h"

/* ===== 센서/판정 파라미터 ===== */
#define SENSOR_PERIOD_NORMAL_MS   1000
#define SENSOR_PERIOD_SLEEP_MS    300

/* ===== 신호 처리 =====
 * MQ-7을 히터 사이클링 없이 상시 5V로 구동하고 예열도 충분치 않아
 * 절대 ADC 값이 매 부팅마다 다른 지점으로 수렴하고 수십 초 단위로 드리프트한다.
 * 따라서 절대 임계값이 아니라 "적응형 기준선 대비 상승량(delta)"으로 판정한다.
 *
 * 필터는 샘플 개수가 아니라 시간 상수 기준(EMA)이다.
 * 이동평균은 창이 샘플 개수라서 모드에 따라 주기가 바뀌면(1000ms <-> 300ms)
 * 실제 시간 창이 3.3배 달라져 필터 특성이 흔들렸다. EMA는 alpha가 dt에
 * 비례해 자동 보정되므로 모드와 무관하게 동일한 시정수를 유지한다. */
#define SIGNAL_TAU_S             4.0f    // 신호 평활화 시정수
#define BASELINE_TAU_S         300.0f    // 기준선 상승 추종 시정수 (5분)

/* 기준선 하강 추종은 상승보다 5배 빠르게 간다.
 * 비대칭인 이유는 안전 방향이 서로 다르기 때문이다.
 *   기준선이 너무 높다 -> delta가 음수로 눌려 경보 감도가 죽는다 (위험)
 *   기준선이 너무 낮다 -> 오경보 쪽으로 기운다 (안전)
 * 대칭으로 두면 자극이 지나간 뒤 기준선이 높은 곳에 남아 회복에 15분이 걸린다
 * (실측: 알코올 노출 후 delta=-36이 유지됨). 이 구간 내내 경보가 둔해진다. */
#define BASELINE_TAU_FALL_S     60.0f    // 기준선 하강 추종 시정수 (1분)

/* 부팅 후 이 시간까지는 경보를 억제하고 기준선을 신호에 하드 고정한다.
 * MQ-7 히터가 켜지면 raw가 400대까지 튀었다가 내려오는데, 이 과도 구간을
 * EMA로 학습하면 기준선이 과도 상태에 물려 이후 delta가 크게 음수로 고착된다. */
#define BASELINE_WARMUP_MS     60000UL

/* delta가 이 값을 넘으면 기준선 학습을 멈춘다.
 * 안 그러면 진짜 CO 누출이 서서히 기준선에 흡수돼 경보가 안 울린다.
 * 단 신호가 기준선보다 낮아지는 방향은 항상 추종한다(영구 오경보 방지). */
#define BASELINE_FREEZE_DELTA      20

/* 동결이 이 시간을 넘기면 학습을 재개한다.
 * 동결만 있으면, 센서가 예열/온도 변화로 서서히 위로 드리프트할 때 기준선이
 * 영원히 멈춰서 가짜 경보가 고착된다(실측에서 40 -> 366까지 흐른 전례 있음).
 * 실제 CO 누출이라면 이 시간 전에 이미 경보가 울려 사용자가 대응한 뒤다. */
#define BASELINE_FREEZE_MAX_MS  600000UL   // 10분

/* 판정 임계값. 단위는 ADC counts 상승량(delta).
 *
 * 실측 근거 (2026-09-02):
 *   평상시 delta 변동 폭 : 1~2 counts 미만 (오버샘플링 64x + EMA 4s 적용 후)
 *   알코올 솜 반응 피크  : 408 counts (로그 peak= 실측)
 *
 * DANGER는 피크의 약 37%로 잡았다. 자극 강도가 절반으로 줄어도 검출되고,
 * 노이즈 바닥 대비로는 약 100배 여유가 있어 오경보 가능성이 낮다.
 * 피크 관측이 1회뿐이라 의도적으로 보수적으로(낮게) 잡은 값이다.
 *
 * WARN은 DANGER의 약 27%. 단순히 노이즈의 몇 배로 잡지 않은 이유는,
 * 실제 오경보원이 변환 노이즈(1~2)가 아니라 기준선 드리프트(수십 counts)이기
 * 때문이다. 한 자리 수로 두면 드리프트만으로 노랑이 계속 켜진다. */
#define CO_DELTA_WARN_NORMAL       40
#define CO_DELTA_DANGER_NORMAL    150
#define CO_DELTA_WARN_SLEEP        28   // NORMAL의 약 70% (FR5 민감도 상향)
#define CO_DELTA_DANGER_SLEEP     105

/* ===== ADC 하드웨어 오버샘플링 (RM0351 18.4.14) =====
 * OVSR 코드: 0=2회 1=4회 2=8회 3=16회 4=32회 5=64회 6=128회 7=256회
 * OVSS는 누적값의 우시프트 비트수. 누적 횟수와 맞추면 출력이 12비트(0~4095)로 유지된다.
 *   16회 누적 -> 4비트 시프트 -> 0~4095 유지, 임계값 재조정 불필요
 * 노이즈가 더 심하면 5(64회)/6으로 올리되 SHIFT도 같이 올릴 것. */
#define ADC_OVERSAMPLE_RATIO_CODE  5U   // 64회 (1회 읽는 데 약 10ms, 주기 300ms에 비해 무시 가능)
#define ADC_OVERSAMPLE_SHIFT       6U   // /64

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