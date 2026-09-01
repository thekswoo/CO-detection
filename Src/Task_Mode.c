#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "config.h"

volatile SystemMode g_mode = MODE_NORMAL;
volatile uint8_t g_muted = 0;

SemaphoreHandle_t xButtonSemaphore;
static volatile uint8_t g_last_pressed_pin = 0;

void Task_Mode(void *pvParameters) {
    for (;;) {
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY) == pdTRUE) {
            switch (g_last_pressed_pin) {
                case 4: g_mode = MODE_SLEEP;  break;              // Button1: 취침진입
                case 5: g_mode = MODE_NORMAL; break;              // Button2: 기상
                case 6: /* FR12 자가진단 트리거, 추후 구현 */ break; // Button3
                case 7: g_muted = !g_muted; break;                // Button4: 음소거 토글
                case 8: NVIC_SystemReset(); break;                // Button5: 비상 리셋
            }
        }
    }
}

/* EXTI4~9 공용 핸들러 (PC4~PC8) */
void EXTI9_5_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    for (int pin = 4; pin <= 8; pin++) {
        if (EXTI->PR1 & (1 << pin)) {
            EXTI->PR1 = (1 << pin);   // 클리어 (write 1 to clear)
            g_last_pressed_pin = pin;
            xSemaphoreGiveFromISR(xButtonSemaphore, &xHigherPriorityTaskWoken);
        }
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
