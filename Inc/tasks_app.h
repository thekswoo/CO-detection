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

/* ===== main.c: 실기 검증 완료된 저수준 드라이버 (CLAUDE.md 2.1) =====
 * 시그니처를 변경하지 말 것. UART_SendString이 const char*가 아닌 것도 원본 그대로 유지. */
void     GPIO_Init(void);
void     ADC_Init(void);
uint16_t ADC_Read(void);
void     USART2_Init(void);
void     UART_SendChar(char c);
void     UART_SendString(char *s);
void     UART_SendNumber(uint16_t num);

/* ===== 애플리케이션 태스크 ===== */
void Task_Heartbeat(void *pvParameters);

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

/* ===== 보드 내장 LED ===== */
#define LD2_PIN                     ( 5U )      /* PA5 (NUCLEO-L476RG User LED) */

#endif /* TASKS_APP_H */
