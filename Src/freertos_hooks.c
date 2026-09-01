/*-----------------------------------------------------------
 * FreeRTOS 애플리케이션 훅
 *
 * Inc/FreeRTOSConfig.h 설정상 아래 두 함수의 정의가 "필수"다.
 * 없으면 컴파일은 통과하고 링크 단계에서 undefined reference가 난다.
 *
 *   configUSE_MALLOC_FAILED_HOOK   1  -> vApplicationMallocFailedHook
 *   configCHECK_FOR_STACK_OVERFLOW 2  -> vApplicationStackOverflowHook
 *
 * 두 훅 모두 UART로 원인을 남기고 정지한다.
 * 조용히 죽는 대신 원인이 시리얼에 찍히므로 이후 단계에서 태스크/큐를 늘릴 때
 * 힙·스택 부족을 즉시 잡아주는 안전망이 된다.
 *-----------------------------------------------------------*/

#include "FreeRTOS.h"
#include "task.h"
#include "tasks_app.h"

/* pvPortMalloc() 실패 시 호출.
 * xTaskCreate / xQueueCreate / xSemaphoreCreate 등이 힙에서 할당에 실패한 경우다.
 * 대응: FreeRTOSConfig.h의 configTOTAL_HEAP_SIZE(현재 20KB)를 늘린다. */
void vApplicationMallocFailedHook(void)
{
    /* UART는 TXE 폴링 방식이라 인터럽트가 꺼져 있어도 정상 동작한다.
     * 커널 자료구조가 더 손상되지 않도록 먼저 인터럽트를 막고 출력한다. */
    taskDISABLE_INTERRUPTS();

    UART_SendString("\r\n[FATAL] pvPortMalloc failed\r\n");
    UART_SendString("        -> configTOTAL_HEAP_SIZE\r\n");

    for (;;) {
        /* 정지. 디버거로 여기 멈춰 있으면 힙 부족이다. */
    }
}

/* 태스크 스택 끝의 워터마크 패턴이 깨졌을 때 호출 (configCHECK_FOR_STACK_OVERFLOW == 2).
 * 대응: 해당 태스크의 xTaskCreate usStackDepth를 늘린다.
 *
 * 주의: 이 훅이 불릴 시점엔 이미 스택이 깨진 상태다. pcTaskName 문자열 자체가
 * 손상됐을 수 있으므로 출력이 깨져 보여도 "스택 오버플로우"라는 사실은 유효하다. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;

    taskDISABLE_INTERRUPTS();

    UART_SendString("\r\n[FATAL] stack overflow: ");
    UART_SendString(pcTaskName);
    UART_SendString("\r\n        -> xTaskCreate usStackDepth\r\n");

    for (;;) {
        /* 정지. */
    }
}
