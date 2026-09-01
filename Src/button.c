/*-----------------------------------------------------------
 * 버튼 입력 — GPIO/EXTI 초기화 + ISR (CLAUDE.md 7장 4단계)
 *
 *   PB5  (아두이노 D4)  -> EXTI5  ┐ EXTI9_5_IRQn
 *   PC7  (아두이노 D9)  -> EXTI7  ┘
 *   PC13 (보드 내장 B1) -> EXTI13   EXTI15_10_IRQn
 *
 * 회로: 버튼 한쪽 다리 -> GPIO, 대각선 다리 -> GND.
 *       외부 저항 없이 MCU 내부 풀업을 쓰므로 active-low다.
 *       (안 누름 = High, 누름 = Low)
 *
 * 참조: RM0351 14장 (EXTI), 9.2.3 (SYSCFG_EXTICRx)
 *-----------------------------------------------------------*/

#include "stm32l476xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "config.h"
#include "tasks_app.h"

/* ISR -> Task_Mode. main.c에서 큐를 먼저 만든 뒤 Button_Init을 부른다. */
extern QueueHandle_t xQueueButton;

typedef struct {
    GPIO_TypeDef *port;
    uint32_t      pin;    /* 핀 번호 = EXTI 라인 번호 */
    ButtonEvent   evt;
} ButtonDef;

static const ButtonDef s_buttons[] = {
    { GPIOB, BTN_MODE_PIN,     BTN_EVT_MODE     },
    { GPIOC, BTN_MUTE_PIN,     BTN_EVT_MUTE     },
    { GPIOC, BTN_SELFTEST_PIN, BTN_EVT_SELFTEST },
};
#define BUTTON_COUNT  (sizeof(s_buttons) / sizeof(s_buttons[0]))

/* 버튼별 눌림 상태와 마지막 상태변화 시각(tick). 디바운스 + 엣지 중복 방지용. */
static volatile uint8_t    s_pressed[BUTTON_COUNT]   = {0};
static volatile TickType_t s_last_tick[BUTTON_COUNT] = {0};

/*
 * 엣지 처리.
 *
 * 상승/하강 엣지를 모두 받되, "어느 엣지가 떴는가"는 신뢰하지 않는다.
 * 접점 바운스 중에는 두 엣지가 뒤섞여 들어오기 때문에, 뗄 때 발생한
 * 하강엣지를 "눌림"으로 오인하면 한 번 눌러 두 번 동작하게 된다.
 *
 * 대신 IDR로 지금 핀의 실제 레벨을 읽고, 눌림<->뗌 상태가 실제로
 * 바뀌었을 때만 반응한다. 이벤트는 "뗌 -> 눌림" 전이에서만 내보낸다.
 */
static void button_edge(uint8_t idx, BaseType_t *woken)
{
    const ButtonDef *b = &s_buttons[idx];
    TickType_t now = xTaskGetTickCountFromISR();

    /* 부호 없는 뺄셈이라 tick이 오버플로해도 차이는 올바르다. */
    if ((TickType_t)(now - s_last_tick[idx]) < pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS)) {
        return;                                  /* 채터링 구간이므로 무시 */
    }

    uint8_t is_low = (b->port->IDR & (1U << b->pin)) ? 0U : 1U;   /* active-low */

    if (is_low && !s_pressed[idx]) {
        s_pressed[idx]   = 1U;
        s_last_tick[idx] = now;
        xQueueSendFromISR(xQueueButton, &b->evt, woken);          /* 누름에서만 */
    } else if (!is_low && s_pressed[idx]) {
        s_pressed[idx]   = 0U;
        s_last_tick[idx] = now;                                   /* 뗌은 상태만 갱신 */
    }
}

/* pending이 걸린 라인을 찾아 처리한다. 두 벡터가 공유한다. */
static void button_service(BaseType_t *woken)
{
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        uint32_t mask = (1U << s_buttons[i].pin);

        if (EXTI->PR1 & mask) {
            EXTI->PR1 = mask;                    /* write-1-to-clear */
            button_edge(i, woken);
        }
    }
}

void Button_Init(void)
{
    /* --- 1. GPIO: 입력 + 내부 풀업 --- */
    RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN);

    /* MODER 00 = 입력. 리셋값에 의존하지 않고 clear로 강제한다 (CLAUDE.md 0.5). */
    GPIOB->MODER &= ~GPIO_MODER_MODE5;
    GPIOC->MODER &= ~(GPIO_MODER_MODE7 | GPIO_MODER_MODE13);

    /* PUPDR 01 = 풀업. 없으면 안 눌렀을 때 핀이 떠서 노이즈로 오동작한다. */
    GPIOB->PUPDR &= ~GPIO_PUPDR_PUPD5;
    GPIOB->PUPDR |=  GPIO_PUPDR_PUPD5_0;

    GPIOC->PUPDR &= ~(GPIO_PUPDR_PUPD7 | GPIO_PUPDR_PUPD13);
    GPIOC->PUPDR |=  (GPIO_PUPDR_PUPD7_0 | GPIO_PUPDR_PUPD13_0);

    /* --- 2. SYSCFG: EXTI 라인마다 어느 포트를 연결할지 선택 --- */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* EXTICR[n]이 라인 4n ~ 4n+3을 담당하고, 각 라인당 4비트. 값: 0=PA, 1=PB, 2=PC */
    SYSCFG->EXTICR[1] &= ~(SYSCFG_EXTICR2_EXTI5 | SYSCFG_EXTICR2_EXTI7);
    SYSCFG->EXTICR[1] |=  (1U << SYSCFG_EXTICR2_EXTI5_Pos)     /* 라인5 -> PB */
                       |  (2U << SYSCFG_EXTICR2_EXTI7_Pos);    /* 라인7 -> PC */

    SYSCFG->EXTICR[3] &= ~SYSCFG_EXTICR4_EXTI13;
    SYSCFG->EXTICR[3] |=  (2U << SYSCFG_EXTICR4_EXTI13_Pos);   /* 라인13 -> PC */

    /* --- 3. EXTI: 양쪽 엣지 --- */
    const uint32_t lines = EXTI_IMR1_IM5 | EXTI_IMR1_IM7 | EXTI_IMR1_IM13;

    /* 누름(하강)과 뗌(상승)을 모두 받아야 눌림 상태를 정확히 추적할 수 있다.
     * 하강만 받으면 뗄 때의 바운스에 섞인 하강엣지를 새 누름으로 오인한다. */
    EXTI->RTSR1 |= lines;
    EXTI->FTSR1 |= lines;

    /* 설정 도중 걸린 pending을 지우고 언마스크.
     * PR1은 write-1-to-clear라 |= 가 아니라 = 로 써야 다른 비트를 안 건드린다. */
    EXTI->PR1  = lines;
    EXTI->IMR1 |= lines;

    /* --- 4. NVIC --- */
    /* configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5.
     * ...FromISR API를 쓰려면 우선순위 숫자가 5 이상(=더 낮은 우선순위)이어야 한다.
     * 어기면 커널 자료구조가 조용히 깨진다 (CLAUDE.md 6장). */
    NVIC_SetPriority(EXTI9_5_IRQn,   6);
    NVIC_SetPriority(EXTI15_10_IRQn, 6);
    NVIC_EnableIRQ(EXTI9_5_IRQn);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* EXTI 라인 5~9 공용 벡터 (PB5, PC7) */
void EXTI9_5_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    button_service(&woken);
    portYIELD_FROM_ISR(woken);
}

/* EXTI 라인 10~15 공용 벡터 (PC13 = 내장 B1) */
void EXTI15_10_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    button_service(&woken);
    portYIELD_FROM_ISR(woken);
}
