#include "stm32l476xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "queue_defs.h"
#include "tasks_app.h"

/* 태스크 간 통신 큐. 정의는 여기, 참조는 각 태스크에서 extern으로 (FR10).
 *   ISR --xQueueButton--> Task_Mode
 *   Task_Sensor --xQueueSensorToJudge--> Task_Judge --xQueueJudgeToOutput--> Task_Output */
QueueHandle_t xQueueButton        = NULL;
QueueHandle_t xQueueSensorToJudge = NULL;
QueueHandle_t xQueueJudgeToOutput = NULL;

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

    /* 하드웨어 오버샘플링 (RM0351 18.4.14)
     *
     * ADSTART를 한 번 세우면 ADC가 스스로 ADC_OVERSAMPLE_N회 연속 변환해
     * 누적한 뒤 우시프트해서 DR에 결과 하나만 남긴다. CPU 개입이 없고
     * ADC_Read()는 그대로 두면 된다.
     *
     * 랜덤 노이즈가 1/sqrt(N)로 줄어든다. 다만 전원 레일 처짐이나 센서
     * 드리프트처럼 모든 샘플에 동일하게 실리는 성분은 못 걸러낸다.
     *
     * 시프트를 누적 횟수와 맞췄으므로 결과는 여전히 0~4095다.
     * -> CO_THRESHOLD_* 값을 재조정할 필요가 없다.
     *
     * CFGR2의 이 비트들은 ADSTART=0 일 때만 쓸 수 있다. 여기는 변환 시작 전이라 안전. */
    ADC1->CFGR2 &= ~(ADC_CFGR2_ROVSE | ADC_CFGR2_OVSR | ADC_CFGR2_OVSS | ADC_CFGR2_TROVS);
    ADC1->CFGR2 |= (ADC_OVERSAMPLE_RATIO_CODE << ADC_CFGR2_OVSR_Pos)
                 | (ADC_OVERSAMPLE_SHIFT      << ADC_CFGR2_OVSS_Pos)
                 |  ADC_CFGR2_ROVSE;
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

    /* 부저 PWM. duty 0(무음)으로 초기화되므로 여기서 소리가 나면 안 된다. */
    TIM2_PWM_Init();
    UART_SendString("[BOOT] tim2 pwm ok\r\n");

    /* 위험도 LED(PB0-2) + 모드 RGB(PC0-2). 전부 소등 상태로 시작. */
    LED_Init();
    UART_SendString("[BOOT] led ok\r\n");

    UART_SendString("[BOOT] FreeRTOS ");
    UART_SendString(tskKERNEL_VERSION_NUMBER);
    UART_SendString("\r\n");

    /* 큐를 EXTI 활성화보다 먼저 만든다.
     * 순서가 반대면 첫 인터럽트가 NULL 큐에 xQueueSendFromISR을 걸어 죽는다. */
    xQueueButton        = xQueueCreate(BUTTON_QUEUE_LEN, sizeof(ButtonEvent));
    xQueueSensorToJudge = xQueueCreate(SENSOR_QUEUE_LEN, sizeof(SensorData_t));
    xQueueJudgeToOutput = xQueueCreate(JUDGE_QUEUE_LEN,  sizeof(JudgeResult_t));

    if (xQueueButton == NULL || xQueueSensorToJudge == NULL || xQueueJudgeToOutput == NULL) {
        UART_SendString("[FATAL] xQueueCreate failed (heap)\r\n");
        for (;;);
    }

    Button_Init();
    UART_SendString("[BOOT] button ok\r\n");

    if (xTaskCreate(Task_Heartbeat, "HB",
                    HEARTBEAT_TASK_STACK, NULL,
                    HEARTBEAT_TASK_PRIORITY, NULL) != pdPASS) {
        UART_SendString("[FATAL] xTaskCreate(HB) failed\r\n");
        for (;;);
    }

    if (xTaskCreate(Task_Mode, "MODE",
                    MODE_TASK_STACK, NULL,
                    MODE_TASK_PRIORITY, NULL) != pdPASS) {
        UART_SendString("[FATAL] xTaskCreate(MODE) failed\r\n");
        for (;;);
    }

    /* 센서 파이프라인: Sensor -> Judge -> Output */
    if (xTaskCreate(Task_Sensor, "SENS",
                    SENSOR_TASK_STACK, NULL,
                    SENSOR_TASK_PRIORITY, NULL) != pdPASS ||
        xTaskCreate(Task_Judge, "JUDG",
                    JUDGE_TASK_STACK, NULL,
                    JUDGE_TASK_PRIORITY, NULL) != pdPASS ||
        xTaskCreate(Task_Output, "OUT",
                    OUTPUT_TASK_STACK, NULL,
                    OUTPUT_TASK_PRIORITY, NULL) != pdPASS) {
        UART_SendString("[FATAL] xTaskCreate(pipeline) failed\r\n");
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
