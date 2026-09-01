#include "stm32l476xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tasks_app.h"

void GPIO_Init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    // PA0: ADC 입력 -> 아날로그 모드 (11)
    GPIOA->MODER &= ~GPIO_MODER_MODE0;
    GPIOA->MODER |= GPIO_MODER_MODE0;

    // ★ L476 전용: 아날로그 스위치 닫기 (이거 없으면 ADC에 신호 안 들어옴)
    GPIOA->ASCR |= GPIO_ASCR_ASC0;

    // PA2(TX), PA3(RX): USART2 -> AF7
    GPIOA->MODER &= ~(GPIO_MODER_MODE2 | GPIO_MODER_MODE3);
    GPIOA->MODER |= (GPIO_MODER_MODE2_1 | GPIO_MODER_MODE3_1);

    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2 | GPIO_AFRL_AFSEL3);
    GPIOA->AFR[0] |= (7 << GPIO_AFRL_AFSEL2_Pos) | (7 << GPIO_AFRL_AFSEL3_Pos);

    // PA5(LD2, heartbeat): 범용 출력 (01)
    // OTYPER=push-pull, PUPDR=none 은 리셋값(0)이 그대로 맞으므로 별도 설정 불필요
    GPIOA->MODER &= ~GPIO_MODER_MODE5;
    GPIOA->MODER |= GPIO_MODER_MODE5_0;
}

void ADC_Init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN;

    // 전압 레귤레이터 켜기 (필수, 안 하면 ADC 안 됨)
    ADC1->CR &= ~ADC_CR_DEEPPWD;
    // ADC 클럭 소스 설정: SYSCLK (AHB 클럭을 그대로 ADC 클럭으로 사용)
    ADC123_COMMON->CCR &= ~ADC_CCR_CKMODE;
    ADC123_COMMON->CCR |= ADC_CCR_CKMODE_1 | ADC_CCR_CKMODE_0; // 11 = HCLK/1 (동기 모드)

    ADC1->CR |= ADC_CR_ADVREGEN;
    for (volatile int i = 0; i < 10000; i++); // 레귤레이터 안정화 대기 (~20us 이상)

    // 캘리브레이션
    ADC1->CR &= ~ADC_CR_ADCALDIF;
    ADC1->CR |= ADC_CR_ADCAL;
    while (ADC1->CR & ADC_CR_ADCAL);

    // ADC 활성화
    ADC1->ISR |= ADC_ISR_ADRDY;
    ADC1->CR |= ADC_CR_ADEN;
    while (!(ADC1->ISR & ADC_ISR_ADRDY));

    // 채널 5 (PA0), 샘플링 타임 설정
    ADC1->SQR1 = (5 << ADC_SQR1_SQ1_Pos); // 시퀀스 1번에 채널5
    ADC1->SMPR1 |= (7 << ADC_SMPR1_SMP5_Pos); // 최대 샘플링타임(느리지만 안정적)
}

uint16_t ADC_Read(void) {
    ADC1->CR |= ADC_CR_ADSTART;
    while (!(ADC1->ISR & ADC_ISR_EOC));
    return (uint16_t)ADC1->DR;
}

void USART2_Init(void) {
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;

    // MSI 4MHz, 115200 baud 기준 BRR 계산
    // BRR = fclk / baud = 4,000,000 / 115200 ≈ 35 (0x23)
    USART2->BRR = 35;

    USART2->CR1 |= USART_CR1_TE; // 송신만 사용
    USART2->CR1 |= USART_CR1_UE; // USART 활성화
}

void UART_SendChar(char c) {
    while (!(USART2->ISR & USART_ISR_TXE));
    USART2->TDR = c;
}

void UART_SendString(char *s) {
    while (*s) UART_SendChar(*s++);
}

void UART_SendNumber(uint16_t num) {
    char buf[6];
    int i = 0;
    if (num == 0) { UART_SendChar('0'); return; }
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    while (i > 0) UART_SendChar(buf[--i]);
}

int main(void) {
    /* 페리페럴 초기화는 스케줄러 시작 전에 모두 끝낸다.
     * 태스크 안에서 초기화하면 초기화 도중 다른 태스크에 선점될 수 있다. */
    GPIO_Init();
    USART2_Init();

    /* ADC_Init보다 먼저 배너를 찍는다.
     * ADC_Init은 캘리브레이션/ADRDY를 while로 폴링하므로, 만약 거기서 멈추면
     * 배너가 나왔는지 여부로 "UART까지는 정상"임을 구분할 수 있다. */
    UART_SendString("\r\n[BOOT] gpio+uart ok\r\n");

    ADC_Init();
    UART_SendString("[BOOT] adc ok\r\n");

    UART_SendString("[BOOT] FreeRTOS ");
    UART_SendString(tskKERNEL_VERSION_NUMBER);
    UART_SendString("\r\n");

    if (xTaskCreate(Task_Heartbeat, "HB",
                    HEARTBEAT_TASK_STACK, NULL,
                    HEARTBEAT_TASK_PRIORITY, NULL) != pdPASS) {
        UART_SendString("[FATAL] xTaskCreate(HB) failed\r\n");
        for (;;);
    }

    UART_SendString("[BOOT] starting scheduler\r\n");
    vTaskStartScheduler();

    /* vTaskStartScheduler는 정상 동작 시 리턴하지 않는다.
     * 여기 도달했다면 아이들/타이머 태스크 생성에 실패한 것이므로
     * configTOTAL_HEAP_SIZE(현재 20KB)를 늘려야 한다. */
    UART_SendString("[FATAL] scheduler returned\r\n");
    for (;;);
}
