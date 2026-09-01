#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "config.h"
#include "queue_defs.h"
#include <stdio.h>

extern QueueHandle_t xQueueJudgeToOutput;
extern volatile uint8_t g_muted;   // FR11

void UART_SendString(char *s);
void UART_SendNumber(uint16_t num);

/* 3색 LED: PB0=초록, PB1=노랑, PB2=빨강 */
static void SetRiskLED(RiskLevel risk) {
    GPIOB->ODR &= ~((1<<0) | (1<<1) | (1<<2));
    switch (risk) {
        case RISK_SAFE:   GPIOB->ODR |= (1<<0); break;
        case RISK_WARN:   GPIOB->ODR |= (1<<1); break;
        case RISK_DANGER: GPIOB->ODR |= (1<<2); break;
    }
}

/* RGB LED(모드 표시): PC0=R, PC1=G, PC2=B, 공통음극 HIGH=점등 */
static void SetModeRGB(SystemMode mode) {
    GPIOC->ODR &= ~((1<<0) | (1<<1) | (1<<2));
    if (mode == MODE_NORMAL) {
        GPIOC->ODR |= (1<<1);            // 초록
    } else {
        GPIOC->ODR |= (1<<2);            // 파랑
    }
}

/* 부저 PWM: TIM2_CH2 (PA1), duty로 세기 조절 */
static void BuzzerOn(uint16_t freq_hz) {
    // TIM2 ARR/CCR 세팅은 TIM2 PWM 초기화 함수에서 이미 구성했다고 가정
    // 여기서는 CCR로 duty만 제어
    extern void TIM2_SetFrequency(uint16_t freq_hz);
    extern void TIM2_SetDuty(uint8_t duty_percent);
    TIM2_SetFrequency(freq_hz);
    TIM2_SetDuty(50);
}
static void BuzzerOff(void) {
    extern void TIM2_SetDuty(uint8_t duty_percent);
    TIM2_SetDuty(0);
}

void Task_Output(void *pvParameters) {
    JudgeResult_t result;

    for (;;) {
        xQueueReceive(xQueueJudgeToOutput, &result, portMAX_DELAY);

        SetRiskLED(result.risk);
        SetModeRGB(result.mode);

        // FR11: 음소거는 부저만 정지, 판정/LED는 그대로 유지
        if (result.alarm_deterministic && !g_muted) {
            uint16_t freq = (result.mode == MODE_SLEEP) ? 3000 : 2000; // 취침시 더 강하게
            BuzzerOn(freq);
        } else {
            BuzzerOff();
        }

        // FR8: UART 로그
        char buf[64];
        snprintf(buf, sizeof(buf), "CO:%u RISK:%d MODE:%d MUTE:%d\r\n",
                 result.adc_raw, result.risk, result.mode, g_muted);
        UART_SendString(buf);
    }
}
