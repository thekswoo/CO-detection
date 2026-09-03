#ifndef TASKS_APP_H
#define TASKS_APP_H

/*-----------------------------------------------------------
 * 애플리케이션 태스크 프로토타입 + 저수준 드라이버 선언 모음.
 *
 * 각 .c 파일에서 extern 선언을 중복으로 적지 않도록 여기 한 곳에 모은다.
 * (기존 Task_*.c들은 파일마다 extern을 따로 적고 있어 시그니처가 어긋날 위험이 있었음)
 *-----------------------------------------------------------*/

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "config.h"

/* ===== main.c: 실기 검증 완료된 저수준 드라이버 (CLAUDE.md 2.1) =====
 * 시그니처를 변경하지 말 것. UART_SendString이 const char*가 아닌 것도 원본 그대로 유지. */
void     GPIO_Init(void);
void     ADC_Init(void);
uint16_t ADC_Read(void);
void     USART2_Init(void);
void     UART_SendChar(char c);
void     UART_SendString(char *s);
void     UART_SendNumber(uint16_t num);

/* ===== tim2_pwm.c: 부저 PWM 드라이버 (TIM2_CH2 / PA1) =====
 * 시그니처는 Src/Task_Output.c가 이미 extern으로 선언해 둔 것과 일치시켜야 한다. */
void TIM2_PWM_Init(void);
void TIM2_SetFrequency(uint16_t freq_hz);
void TIM2_SetDuty(uint8_t duty_percent);

/* ===== led.c: 위험도 LED(GPIOB) + 모드 RGB LED(GPIOC) ===== */
void LED_Init(void);
void LED_SetRisk(RiskLevel risk);      /* 초록/노랑/빨강 배타적 점등 */
void LED_SetMode(SystemMode mode);     /* RGB: 일반=초록, 취침=파랑 */
void LED_AllOff(void);
void LED_SelfTest(void);               /* FR12 자가진단 시퀀스 (스케줄러 실행 중에만 호출) */

/* ===== button.c: EXTI 버튼 입력 ===== */
void Button_Init(void);

/* ===== tmp102.c: 온도센서 (FR6, 8단계에서 구현) ===== */
int16_t TMP102_ReadTempX10(void);   /* 0.1도 단위 정수 */

/* ===== mlp.c: 조기경보 추론 (FR7, 9단계에서 구현) ===== */
float mlp_infer(float co_val, float rate_of_rise);   /* 0.0~1.0 위험 확률 */

/* ===== 애플리케이션 태스크 ===== */
void Task_Heartbeat(void *pvParameters);
void Task_Mode(void *pvParameters);
void Task_Sensor(void *pvParameters);
void Task_Judge(void *pvParameters);
void Task_Output(void *pvParameters);

/* 이후 단계에서 순차적으로 추가한다 (CLAUDE.md 7장).
 * 현재 Src/Task_Sensor.c / Task_Judge.c / Task_Output.c / Task_Mode.c 는
 * 미구현 심볼을 참조하므로 빌드에서 제외된 상태다.
 *
 * void Task_Sensor(void *pvParameters);
 * void Task_Judge (void *pvParameters);
 * void Task_Output(void *pvParameters);
 * void Task_Mode  (void *pvParameters);
 */

/* ===== 태스크 스택 / 우선순위 (CLAUDE.md 3.1) =====
 * 스택은 word 단위. configMINIMAL_STACK_SIZE = 128 word = 512 byte.
 * Task_Heartbeat는 UART 폴링 출력만 하므로 최소 스택으로 충분하다. */
#define HEARTBEAT_TASK_STACK        configMINIMAL_STACK_SIZE
#define HEARTBEAT_TASK_PRIORITY     ( 1 )       /* 가장 낮음. 이후 추가될 태스크에 항상 양보 */
#define HEARTBEAT_PERIOD_MS         ( 500 )
/* LED 점멸은 500ms 그대로 두고 UART 로그만 솎아낸다.
 * 5단계부터 [CO] 로그가 1초마다 나오므로 [HB]까지 매번 찍으면 읽기 어렵다. */
#define HEARTBEAT_LOG_EVERY_N       ( 10 )      /* 10 * 500ms = 5초마다 */

/* ===== 보드 내장 LED ===== */
#define LD2_PIN                     ( 5U )      /* PA5 (NUCLEO-L476RG User LED) */

/* ===== 외부 LED 핀 (CLAUDE.md 1.2 핀맵) =====
 * 위험도 LED: GPIOB, 애노드 구동 (GPIO High = 점등)
 * 모드 RGB : GPIOC, 공통음극  (GPIO High = 점등) */
#define LED_GREEN_PIN               ( 0U )      /* PB0 = 아두이노 A3 */
#define LED_YELLOW_PIN              ( 1U )      /* PB1 = 모포 헤더 */
#define LED_RED_PIN                 ( 2U )      /* PB2 = 모포 헤더 */
#define LED_RISK_MASK               ( (1U << LED_GREEN_PIN) | (1U << LED_YELLOW_PIN) | (1U << LED_RED_PIN) )

#define RGB_R_PIN                   ( 0U )      /* PC0 = 아두이노 A5 */
#define RGB_G_PIN                   ( 1U )      /* PC1 = 아두이노 A4 */
#define RGB_B_PIN                   ( 2U )      /* PC2 = 모포 헤더 */
#define RGB_MASK                    ( (1U << RGB_R_PIN) | (1U << RGB_G_PIN) | (1U << RGB_B_PIN) )

#define LED_SELFTEST_STEP_MS        ( 300 )     /* 자가진단 각 단계 점등 시간 */

/* ===== 버튼 입력 (CLAUDE.md 1.2 핀맵) =====
 * 전부 active-low + 내부 풀업. EXTI 라인 번호가 서로 달라야 충돌하지 않는다.
 *   PB5  = 아두이노 D4  -> EXTI5   ┐ 둘 다 EXTI9_5_IRQn
 *   PC7  = 아두이노 D9  -> EXTI7   ┘
 *   PC13 = 보드 내장 B1 -> EXTI13    EXTI15_10_IRQn
 * 비상 리셋은 보드 내장 RESET 버튼(하드웨어)이 담당하므로 GPIO를 쓰지 않는다. */
#define BTN_MODE_PIN                ( 5U )      /* PB5,  모드 토글 */
#define BTN_MUTE_PIN                ( 7U )      /* PC7,  경보 음소거 */
#define BTN_SELFTEST_PIN            ( 13U )     /* PC13, 자가진단 (B1) */

/* 큐로 주고받는 버튼 이벤트 ID */
typedef enum {
    BTN_EVT_MODE     = 0,
    BTN_EVT_MUTE     = 1,
    BTN_EVT_SELFTEST = 2
} ButtonEvent;

#define BUTTON_QUEUE_LEN            ( 8 )

#define MODE_TASK_STACK             ( configMINIMAL_STACK_SIZE * 2 )  /* SelfTest 호출로 스택 여유 필요 */
#define MODE_TASK_PRIORITY          ( 2 )       /* Heartbeat(1)보다 높고 Sensor/Judge(3)보다 낮음 */

/* ===== 센서 파이프라인 태스크 (CLAUDE.md 3.1) =====
 * Sensor/Judge를 높게 두는 이유: 감지·판정이 표시보다 항상 우선해야 한다.
 * 출력이 밀리는 건 괜찮지만 판정이 밀리면 안 된다. */
#define SENSOR_TASK_STACK           ( configMINIMAL_STACK_SIZE * 2 )
#define SENSOR_TASK_PRIORITY        ( 3 )
#define JUDGE_TASK_STACK            ( configMINIMAL_STACK_SIZE * 2 )
#define JUDGE_TASK_PRIORITY         ( 3 )
#define OUTPUT_TASK_STACK           ( configMINIMAL_STACK_SIZE * 2 )
#define OUTPUT_TASK_PRIORITY        ( 2 )

#define SENSOR_QUEUE_LEN            ( 4 )
#define JUDGE_QUEUE_LEN             ( 4 )

/* ===== 2단계 임시 검증용 =====
 * Task_Heartbeat가 주기적으로 짧게 삑 소리를 내 부저 회로를 확인한다.
 * 3단계 이후 Task_Output이 부저를 넘겨받으면 0으로 바꿔 끌 것. */
/* 2단계 검증 완료. 4단계부터는 버튼 확인음과 겹쳐 헷갈리므로 끈다. */
#define HEARTBEAT_BEEP_TEST         ( 0 )

/* 진단용: heartbeat 줄에 버튼 핀의 raw 입력값(D4/D9/B1)을 같이 찍는다.
 * 내부 풀업이라 안 누름=1, 누름=0 이어야 정상.
 * 버튼이 안 먹을 때 "배선 문제"와 "EXTI 설정 문제"를 구분하는 용도로 1로 켤 것.
 * (4단계에서 실제로 이걸로 버튼 상시단락을 잡았다) */
#define HEARTBEAT_PIN_DEBUG         ( 0 )
#define HEARTBEAT_BEEP_EVERY_N      ( 10 )      /* 10 * 500ms = 5초마다 */
#define HEARTBEAT_BEEP_MS           ( 80 )

/* 3단계 임시 검증용이었던 LED 순환 테스트.
 * 4단계부터 버튼(Task_Mode)이 LED를 제어하므로 끈다. */
#define HEARTBEAT_LED_TEST          ( 0 )

#endif /* TASKS_APP_H */
