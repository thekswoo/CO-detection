#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * STM32L476RG (Cortex-M4F, 80MHz max, SRAM 96KB) 기준 설정
 * 불침번 프로젝트 (CO-detection) 용
 *-----------------------------------------------------------*/

/* ===== 기본 커널 설정 ===== */
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      ( 4000000UL )  /* MSI 4MHz 기준. 나중에 클럭 올리면 반드시 수정 */
#define configTICK_RATE_HZ                      ( 1000 )       /* 1ms tick */
#define configMAX_PRIORITIES                    ( 5 )
#define configMINIMAL_STACK_SIZE                ( 128 )        /* word 단위 (128*4=512byte) */
#define configTOTAL_HEAP_SIZE                   ( 20 * 1024 )  /* 20KB, SRAM 96KB 중 일부. 태스크 늘어나면 조정 */
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 0

/* ===== Hook 함수 ===== */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2   /* 디버깅 중엔 2 권장, 스택오버플로우 즉시 감지 */

/* ===== Run time / Task stats (디버깅용, 필요시 1로) ===== */
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/* ===== 소프트웨어 타이머 (자가진단 FR12 등에서 쓸 수 있음) ===== */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( 3 )
#define configTIMER_QUEUE_LENGTH                 10
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

/* ===== 코루틴 (안 씀) ===== */
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         ( 2 )

/* ===== API 함수 포함 여부 ===== */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources            1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_xTaskGetIdleTaskHandle           0
#define INCLUDE_eTaskGetState                    1
#define INCLUDE_xTimerPendFunctionCall           0
#define INCLUDE_xTaskAbortDelay                  0
#define INCLUDE_xTaskGetHandle                   0
#define INCLUDE_xSemaphoreGetMutexHolder         0

/* ===== Cortex-M4F 인터럽트 우선순위 설정 =====
 * L476은 4bit 전체를 preemption priority로 사용 (subpriority 없음, priority group 4)
 * 값이 클수록 우선순위 낮음. 0이 가장 높음(NMI/HardFault 등 커널 예약 영역 침범 금지)
 */
#define configPRIO_BITS                         4

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY     5

#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* configMAX_SYSCALL_INTERRUPT_PRIORITY 보다 우선순위 낮은(숫자 큰) 인터럽트에서만
 * FreeRTOS API(...FromISR) 호출 가능. EXTI9_5_IRQn 우선순위 설정 시 이 값 이하로 맞출 것 */

/* ===== Assert ===== */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* ===== 핸들러 이름 매핑 (CMSIS 벡터 테이블과 연결) =====
 * startup_stm32l476rgtx.s 에 이미 SVC_Handler/PendSV_Handler/SysTick_Handler가
 * weak 심볼로 정의되어 있음. FreeRTOS port.c가 같은 이름으로 정의하면 링커가
 * FreeRTOS 쪽을 채택함 (weak override). 별도 매핑 불필요.
 */
#define vPortSVCHandler      SVC_Handler
#define xPortPendSVHandler   PendSV_Handler
#define xPortSysTickHandler  SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
