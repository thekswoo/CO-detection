# 불침번 (CO-detection) — 프로젝트 명세

차박/캠핑카 환경의 일산화탄소(CO) 중독 방지 시스템.
제24회 임베디드SW경진대회 자유공모 부문 출품작 (예선 마감: 2026-09-03).

---

## 0. 절대 규칙 (Non-negotiable)

이 프로젝트는 **레지스터 레벨 베어메탈** 개발이다. 아래를 반드시 지킬 것.

1. **HAL 라이브러리 사용 금지.** `HAL_*` 함수, `stm32l4xx_hal*.h` 인클루드 금지.
2. **CubeMX 코드 생성기 사용 금지.** `/* USER CODE BEGIN */` 주석 블록 만들지 말 것.
3. 모든 페리페럴 설정은 **CMSIS 레지스터 직접 접근**으로 작성한다.
   - 예: `RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;`
4. 참조 문서는 **RM0351** (STM32L4x5/L4x6 Reference Manual). 추측하지 말고 레지스터 정의를 확인할 것.
5. 비트 조작 시 **clear-before-set 패턴 필수**.
   ```c
   REG &= ~FIELD_MASK;
   REG |= (value << FIELD_Pos);
   ```
   리셋값이 0이 아닌 레지스터가 존재하므로 `|=`만 쓰면 안 된다.
6. FreeRTOS는 **수동 통합**되어 있다. CMSIS-RTOS 래퍼(`osThreadNew` 등) 사용 금지, **네이티브 FreeRTOS API**(`xTaskCreate`, `xQueueSend` 등)만 사용한다.

---

## 1. 하드웨어

### 1.1 보드 / MCU

- **NUCLEO-L476RG** (STM32L476RG, Cortex-M4F, FPU 내장)
- SRAM 96KB, Flash 1MB
- 현재 시스템 클럭: **MSI 4MHz** (PLL 미설정 상태)
  - `FreeRTOSConfig.h`의 `configCPU_CLOCK_HZ`가 4000000UL로 맞춰져 있음
  - UART BRR도 4MHz 기준으로 계산되어 있음 (`USART2->BRR = 35` @115200)
  - **클럭을 변경할 경우 위 두 곳을 반드시 함께 수정할 것**

### 1.2 핀맵 (확정)

| 기능 | 핀 | 페리페럴 | 비고 |
|---|---|---|---|
| MQ-7 CO 센서 입력 | PA0 | ADC1_IN5 | 전압분배 통과 후 |
| 부저 PWM | PA1 | TIM2_CH2 | NPN 트랜지스터 베이스 구동 |
| UART TX | PA2 | USART2 (AF7) | 로그 출력 |
| UART RX | PA3 | USART2 (AF7) | 미사용 (TE만 활성) |
| 위험도 LED 초록 | PB0 | GPIO 출력 | RISK_SAFE |
| 위험도 LED 노랑 | PB1 | GPIO 출력 | RISK_WARN |
| 위험도 LED 빨강 | PB2 | GPIO 출력 | RISK_DANGER |
| I2C SCL | PB8 | I2C1 | LCD + TMP102 공용 |
| I2C SDA | PB9 | I2C1 | LCD + TMP102 공용 |
| 모드 RGB LED — R | PC0 | GPIO 출력 | 공통음극, HIGH=점등 |
| 모드 RGB LED — G | PC1 | GPIO 출력 | 일반모드 표시 |
| 모드 RGB LED — B | PC2 | GPIO 출력 | 취침모드 표시 |
| Button 1 (모드 토글) | PB5 | EXTI5 | 아두이노 `D4`, 내부 풀업, active-low |
| Button 2 (경보 음소거) | PC7 | EXTI7 | 아두이노 `D9`, 내부 풀업, active-low |
| B1 (자가진단) | PC13 | EXTI13 | **보드 내장 파란 버튼**, 배선 불필요 |
| RESET (비상 리셋) | — | — | **보드 내장 검정 버튼**, GPIO 미사용 |

**주의:** EXTI는 포트가 달라도 핀 번호가 같으면 라인을 공유한다. 위 배정(5 / 7 / 13)은 번호 중복이 없다.
라인 5·7은 `EXTI9_5_IRQn`, 라인 13은 `EXTI15_10_IRQn`으로 벡터가 나뉜다.

> **설계 변경 이력:** 원래 물리 버튼 5개(PC4~PC8)였으나 4단계에서 2개로 축소했다.
> ① 비상 리셋은 보드 내장 RESET 버튼이 하드웨어로 동일한 일을 하고, CO 경보 중 `NVIC_SystemReset()`은
> 경보 중단·센서 상태 손실을 유발해 안전상 역효과였다. ② 자가진단은 유휴 상태였던 내장 B1에 할당했다.
> ③ 취침 진입/복귀는 RGB LED가 현재 모드를 항상 표시하므로 단일 토글로 통합했다.
> 결과적으로 FR4의 5개 기능은 모두 유지되면서 배선과 EXTI 벡터가 단순해졌다.

### 1.3 주변 회로

**MQ-7 전압분배 (검증 완료)**
```
MQ-7 AOUT ──[10kΩ]──┬──[10kΩ]── GND
                     └── PA0 (ADC)
```
- 1:1 분배로 5V → 2.5V. MQ-7 AOUT은 VCC(5V) 기준 출력이므로 3.3V ADC 보호를 위해 필수.
- 요구사항 원문에는 20kΩ/20kΩ으로 기재되어 있으나, 실제 배선은 10kΩ/10kΩ이다. **분배비가 동일하므로 전기적으로 등가.**

**부저 구동 (KSP2222ABU, TO-92)**
```
PA1 (TIM2_CH2 PWM) ──[330Ω]── B
                                C ── 부저(-)
                                E ── GND
부저(+) ── 5V
```
- 핀 배열: 평평한 면 정면, 다리 아래 기준 좌→우 **E - B - C**

**I2C 슬레이브**
| 장치 | 주소 | VCC |
|---|---|---|
| GDM1602K LCD (PCF8574 백팩) | 0x27 | 5V |
| TMP102 온도센서 | 0x48 | **3.3V** |

> TMP102는 절대최대정격 3.6V다. LCD 백팩에 5V 풀업이 내장된 경우 I2C 라인이 5V로 올라가 TMP102가 손상될 수 있다. 배선 전 LCD 백팩의 풀업 저항 연결 확인 필요. 확인 전까지는 TMP102를 버스에서 분리해두고 개발할 것.

---

## 2. 현재 코드 상태

### 2.1 검증 완료된 코드 (`Src/main.c`)

아래 함수들은 **실기 검증이 끝났다. 동작 로직을 변경하지 말 것.** 리팩터링 시 시그니처와 레지스터 시퀀스를 그대로 유지한다.

- `GPIO_Init()` — PA0 아날로그, PA2/PA3 AF7
- `ADC_Init()` — DEEPPWD 해제 → CCR CKMODE → ADVREGEN → 안정화 대기 → 캘리브레이션 → ADEN → ADRDY 대기 → SQR1(채널5) → SMPR1
- `ADC_Read()` — ADSTART → EOC 폴링 → DR 반환
- `USART2_Init()`, `UART_SendChar()`, `UART_SendString()`, `UART_SendNumber()`

**검증 데이터:**
| 입력 | ADC 출력 |
|---|---|
| GND 직결 | 0 |
| 3.3V 직결 | 4091~4093 |
| 대기 중 MQ-7 (예열 후) | 61~73 |
| 알코올 솜 반응 피크 | 608 |

### 2.2 반드시 알아야 할 실리콘 함정

**`GPIOx_ASCR` (STM32L47x/48x 전용)**

L476은 GPIO 핀과 ADC 입력 사이에 아날로그 스위치가 존재한다. `MODER`를 아날로그 모드(11)로 설정하는 것만으로는 **ADC에 신호가 전달되지 않는다.**

```c
GPIOA->MODER &= ~GPIO_MODER_MODE0;
GPIOA->MODER |= GPIO_MODER_MODE0;
GPIOA->ASCR |= GPIO_ASCR_ASC0;   // ★ 이 줄이 없으면 ADC 값이 상수로 고정됨
```

- 증상: 캘리브레이션 통과, ADRDY 정상, EOC 매 변환마다 정상 셋 → **변환은 완벽히 동작하는데 값만 상수(약 70)로 고정.** 폴링 루프가 멈추지 않아 "ADC가 죽었다"는 형태로 나타나지 않는다.
- L496 이후 계열에는 이 레지스터가 없다(자동 처리). F7xx 계열에는 개념 자체가 없다.
- **ADC 채널을 추가할 때마다 해당 핀의 ASC 비트를 반드시 함께 설정할 것.**

**기타 L4 계열 주의사항**
- GPIO 클럭은 **AHB2**(`RCC->AHB2ENR`). F4/F7의 AHB1이 아니다.
- ADC 클럭도 AHB2 (`RCC_AHB2ENR_ADCEN`).
- `ADC123_COMMON->CCR`의 CKMODE를 설정하지 않으면 캘리브레이션이 무한루프에 빠진다.

### 2.3 FreeRTOS 통합 상태

수동 통합 완료. 디렉토리 구조:

```
Detect-CO/
├── Inc/
│   ├── FreeRTOSConfig.h        (작성 완료)
│   ├── config.h                (신규 작성 필요)
│   └── queue_defs.h            (신규 작성 필요)
├── Src/
│   ├── main.c                  (기존, 재구성 필요)
│   ├── Task_Sensor.c           (신규)
│   ├── Task_Judge.c            (신규)
│   ├── Task_Output.c           (신규)
│   └── Task_Mode.c             (신규)
└── Middlewares/FreeRTOS/
    ├── croutine.c, event_groups.c, list.c, queue.c,
    │   stream_buffer.c, tasks.c, timers.c
    ├── include/                (커널 헤더 전체)
    └── portable/
        ├── GCC/ARM_CM4F/       (port.c, portmacro.h)
        └── MemMang/heap_4.c
```

**포트는 반드시 `ARM_CM4F`** (FPU 지원). `ARM_CM4`(F 없음)를 쓰면 FPU 컨텍스트 저장이 누락되어 부동소수점 연산 태스크에서 크래시가 발생한다. MLP 추론이 float 연산이므로 특히 중요.

`FreeRTOSConfig.h`에서 `SVC_Handler`/`PendSV_Handler`/`SysTick_Handler`를 `port.c`의 구현으로 매핑해두었다. `startup_stm32l476rgtx.s`의 weak 심볼이 자동으로 오버라이드된다.

**빌드 설정 (아직 미완료일 수 있음 — 확인 필요)**
- Include path에 `../Middlewares/FreeRTOS/include`, `../Middlewares/FreeRTOS/portable/GCC/ARM_CM4F` 추가
- `Middlewares` 폴더가 소스 빌드 대상에 포함되어 있는지 확인
- 프리프로세서 매크로 `STM32L476xx` 정의 필요 (Empty Project라 수동 추가 대상)

---

## 3. 시스템 아키텍처

### 3.1 태스크 구성

| 태스크 | 역할 | 트리거 | 우선순위 |
|---|---|---|---|
| `Task_Sensor` | ADC 읽기, 이동평균 필터, 상승률 계산, TMP102 읽기 | 주기 (모드에 따라 가변) | 3 (높음) |
| `Task_Judge` | 임계값 판정 + MLP 추론, 위험도 레벨 결정 | Sensor 큐 수신 시 | 3 (높음) |
| `Task_Output` | LED/RGB/부저 제어, LCD 갱신, UART 로그 | Judge 큐 수신 시 | 2 (중간) |
| `Task_Mode` | 버튼 이벤트 처리 (모드 전환, 음소거, 자가진단) | 큐 `xQueueButton` (EXTI ISR에서 send) | 2 (중간) |

### 3.2 데이터 흐름

```
[EXTI ISR] --xSemaphoreGiveFromISR--> Task_Mode
                                          |
                                    (g_mode, g_muted 갱신)
                                          |
                                          v
Task_Sensor --xQueueSensorToJudge--> Task_Judge --xQueueJudgeToOutput--> Task_Output
```

### 3.3 공유 자료구조

```c
/* config.h */
typedef enum { MODE_NORMAL = 0, MODE_SLEEP = 1 } SystemMode;
typedef enum { RISK_SAFE = 0, RISK_WARN = 1, RISK_DANGER = 2 } RiskLevel;

#define SENSOR_PERIOD_NORMAL_MS   1000
#define SENSOR_PERIOD_SLEEP_MS     300
#define CO_THRESHOLD_NORMAL        500
#define CO_THRESHOLD_SLEEP         350   /* NORMAL의 70% */
#define MOVING_AVG_WINDOW            4

/* queue_defs.h */
typedef struct {
    uint16_t adc_raw;        /* 이동평균 적용 후 */
    float    rate_of_rise;   /* MLP 입력용, ADC counts/sec */
    int16_t  temp_c_x10;     /* TMP102, 0.1℃ 단위 */
} SensorData_t;

typedef struct {
    uint16_t   adc_raw;
    RiskLevel  risk;
    uint8_t    alarm_deterministic;  /* 임계값 초과 */
    uint8_t    alarm_mlp;            /* MLP 조기경보 */
    SystemMode mode;
} JudgeResult_t;
```

전역 상태 (`volatile`, Task_Mode가 write / 나머지가 read):
```c
volatile SystemMode g_mode;    /* MODE_NORMAL 초기값 */
volatile uint8_t    g_muted;   /* 0 초기값 */
```

---

## 4. 기능 요구사항

### 4.1 필수 (Must)

| ID | 내용 | 담당 태스크 |
|---|---|---|
| FR1 | MQ-7로 CO 농도를 주기적으로 측정 (ADC) | Task_Sensor |
| FR2 | 임계값 초과 시 즉시 경보 (부저 + 빨강 LED) | Task_Judge → Task_Output |
| FR3 | LCD에 실시간 표시, 3색 LED로 위험도 3단계 시각화 | Task_Output |
| FR4 | 버튼 입력: 취침 진입/해제, 자가진단, 음소거, 비상 리셋 (물리 버튼 2개 + 보드 내장 B1/RESET) | Task_Mode |
| FR5 | 취침모드 시 감지 주기 단축 + 임계값 민감도 상향 + 경보 강도 최대 | 전체 |
| FR9 | RGB LED로 동작 모드(일반/취침) 표시 | Task_Output |
| FR10 | FreeRTOS 멀티태스킹 (태스크 3개 이상, 큐/세마포어 통신) | 전체 |

### 4.2 차별화 기능 (Should)

| ID | 내용 |
|---|---|
| FR6 | TMP102 온도는 **UART 로그용 보조 지표로만** 사용. MLP 입력에 포함하지 않는다 (온도-CO중독 간 인과관계가 약하다고 판단해 의도적으로 배제) |
| FR7 | CO 농도 + CO 상승 기울기(2입력) MLP를 C로 추론. 조기경보를 노랑 LED로 표시 |
| FR8 | UART로 전체 로그 실시간 출력 (시연/디버깅용) |
| FR11 | **음소거 시 부저만 정지.** CO 감지·판정·LED 표시는 계속 유지 (안전상 판정 로직은 절대 정지 금지) |
| FR12 | 자가진단 버튼: LED 3색 + RGB + 부저를 순차 구동해 하드웨어 정상 여부 확인 |

### 4.3 버튼 기능 매핑

| 입력 | 핀 | 아두이노 | 기능 |
|---|---|---|---|
| 버튼1 | PB5 | `D4` | 모드 토글 (`g_mode` 반전: 일반 ↔ 취침) |
| 버튼2 | PC7 | `D9` | 경보 음소거 토글 (`g_muted` 반전) |
| B1 (내장) | PC13 | — | 수동 자가진단 (FR12 시퀀스 트리거) |
| RESET (내장) | — | — | 비상 리셋 (하드웨어 리셋) |

ISR은 큐(`xQueueButton`)로 `ButtonEvent`를 넘기고, 실제 상태 변경·UART 로그·부저 확인음은 전부 `Task_Mode`에서 처리한다.
세마포어 + 전역 하나였던 이전 구조는 두 버튼이 연달아 눌리면 먼저 것이 덮어써져 유실되므로 큐로 교체했다.

### 4.4 위험도 판정 로직

```
alarm_deterministic (임계값 초과)  → RISK_DANGER  (빨강 + 부저)
alarm_mlp (조기경보)               → RISK_WARN    (노랑)
둘 다 아님                         → RISK_SAFE    (초록)
```
결정론적 경보가 MLP보다 우선한다. MLP가 오동작해도 임계값 로직은 독립적으로 항상 동작해야 한다.

### 4.5 제외 범위 (Won't)

- 무선 통신, 모바일 앱 연동
- PCB 설계, 케이스 제작 (예선 불필요)

---

## 5. MLP (FR7) 구현 방침

### 5.1 학습/추론 분리

- **학습은 PC(Python)에서**, **추론만 STM32에서** 수행한다.
- 학습된 가중치는 `Inc/mlp_weights.h`에 `const float` 배열로 하드코딩한다.
- STM32에 학습 코드를 올리지 않는다.

### 5.2 네트워크 구조

```
입력층(2) → 은닉층(4~8, ReLU) → 출력층(1, sigmoid)
```
- 입력: `[정규화된 CO 농도, 정규화된 상승률]`
- 출력: 0~1 위험 확률. 0.5 초과 시 `alarm_mlp = 1`

### 5.3 학습 데이터

**실측 CO 측정기가 없으므로, MQ-7 데이터시트의 Rs/R0 vs ppm 곡선 기반 합성 데이터를 사용한다.**
- 실측 캘리브레이션은 결선(11월) 단계로 연기
- 보고서에 이 사실을 명시하고 향후 계획으로 기술

### 5.4 정규화 주의사항

학습 시 적용한 정규화 방식을 **추론 코드에도 정확히 동일하게 적용해야 한다.** 이것이 임베디드 ML에서 가장 흔한 버그 원인이다. 정규화 파라미터(min/max 또는 mean/std)도 `mlp_weights.h`에 함께 export할 것.

---

## 6. 인터럽트 우선순위 규칙

`FreeRTOSConfig.h` 설정:
```c
#define configPRIO_BITS                              4
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
```

**FreeRTOS API(`...FromISR`)를 호출하는 인터럽트는 우선순위 숫자가 5 이상이어야 한다.** (숫자가 클수록 낮은 우선순위)

버튼 EXTI 설정 예:
```c
NVIC_SetPriority(EXTI4_IRQn,   6);   /* 5 이상 */
NVIC_SetPriority(EXTI9_5_IRQn, 6);
NVIC_EnableIRQ(EXTI4_IRQn);
NVIC_EnableIRQ(EXTI9_5_IRQn);
```

이 규칙을 어기면 인터럽트 중 커널 자료구조가 손상되어 재현이 어려운 버그가 발생한다.

---

## 7. 구현 우선순위

의존관계 순으로, **각 단계마다 빌드+실기 검증이 가능한 단위**로 진행할 것.

1. **FreeRTOS 빌드 검증** — 태스크 없이 커널만 링크되는지 확인. 이후 LED 토글 태스크 1개로 스케줄러 동작 확인
2. **TIM2 PWM 초기화** (`TIM2_PWM_Init`, `TIM2_SetFrequency`, `TIM2_SetDuty`) — 부저 소리 확인
3. **GPIO 출력 초기화** (PB0-2, PC0-2) — LED 점등 확인
4. **EXTI 초기화** (PC4-8, 풀업, 트리거 엣지, NVIC) — 버튼 입력 확인
5. **Task_Sensor** → **Task_Judge** → **Task_Output** 순서로 연결. 이 시점에 FR1/FR2/FR3(LED)/FR8/FR10 동작
6. **Task_Mode** 연결 → FR4/FR5/FR9/FR11 동작
7. **FR12 자가진단 시퀀스**
8. **I2C1 + LCD** (FR3 나머지) — TMP102는 풀업 확인 후 연결 (FR6)
9. **MLP 학습 + 추론 통합** (FR7)

---

## 8. 코딩 컨벤션

- 함수명: `Peripheral_Action()` (예: `ADC_Read`, `TIM2_SetDuty`)
- 태스크 함수: `Task_Xxx(void *pvParameters)`
- 전역 변수: `g_` 접두사, 공유 시 `volatile`
- 상수: 대문자 스네이크, `#define`
- 주석: 한국어 허용. **레지스터 조작에는 왜 그렇게 하는지 근거를 남길 것** (특히 RM0351 참조 부분)
- 하드코딩된 매직넘버 금지. `config.h`의 `#define`으로 뺄 것

---

## 9. 저장소

- GitHub: `thekswoo/CO-detection`
- 커밋은 터미널 git 사용 (CubeIDE EGit 미사용)
