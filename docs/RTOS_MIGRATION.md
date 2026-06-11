# ROKI RTOS 재구축 마이그레이션 / 아키텍처 결정 문서

> 이 문서는 기존 `ROKI_U375`(슈퍼루프 기반)를 **RTOS-native 새 프로젝트**로
> 다시 구축할 때의 설계 결정·폴더 구조·이식 규칙을 박제한 것이다.
> 새 프로젝트 repo의 `docs/`에 복사해서 작업 시작점으로 사용한다.
> AI 협업 시: **"이 문서를 먼저 읽고 그 규칙대로 작업하라"** 고 지시할 것.

---

## 0. 결정 배경 (왜 새로 짜는가)

- **MCU 동일**: STM32U375RGTx 유지. (RTOS/커널/클럭 구조는 재사용 가능)
- **핀·회로·기능 변경 예정**: 새 PCB가 아직 안 떴고, 핀맵/주변장치 배선/기능이 바뀔 것.
  → 기존 프로젝트의 최대 자산인 "검증된 핀 통합"이 어차피 무효화됨.
- **RTOS-native 목표**: `ap_main()` 같은 거대한 `while(1)` 슈퍼루프를 버리고,
  태스크/IPC 기반으로 재설계.
- **결론**: 기존 프로젝트에 RTOS를 욱여넣기보다, **새 프로젝트로 fresh start**.
  드라이버/검증 알고리즘은 복사해 오되, 핀 의존부는 bsp로 추출하고
  기능 로직은 드라이버 직접 호출을 끊어 인터페이스 뒤로 보낸다.

---

## 1. 핵심 설계 원칙

1. **레이어 분리**: 하드웨어 향하는 층(`UserDrivers/`)과 응용 층(`App/`)을 분리.
2. **슈퍼루프 금지**: 각 태스크는 고정주기(`osDelayUntil`) 또는 이벤트 블로킹으로 동작.
3. **단일 변경 진입점**: PCB가 바뀌면 `UserDrivers/bsp/board.h` (+ 드라이버 구현)만 수정.
4. **features는 드라이버 직접 호출 금지**: `step_drive()` 등을 직접 부르지 말고
   `services/motion_ctrl` 또는 `hal_if`(추상 인터페이스) 경유.
5. **전역변수 공유 금지**: 태스크 간 데이터는 `App/ipc/`의 큐/notification/뮤텍스로만.
6. **"무엇을 하나(services)"와 "언제·누가 하나(tasks)"를 분리**.
7. **코딩 컨벤션**: snake_case (함수/변수/구조체 멤버). 타입은 `_t` 접미사, 매크로는 UPPER_SNAKE_CASE.

### 의존성 방향 (한 방향으로만)
```
App/app → App/tasks → App/features → App/services → UserDrivers/hal_if
                                                          ↑
                              UserDrivers/drivers ────────┘ → UserDrivers/bsp
```
- 위 계층이 아래를 호출. 아래는 위를 모름.
- `bsp`만 실제 핀/HAL을 안다.
- `App/*`는 `hal_if` 인터페이스만 보고 실제 핀은 모른다.

---

## 2. 개념 정의

### hal_if (Hardware Abstraction Layer Interface)
- "무엇을 할 수 있나"만 정의한 **헤더(계약)**. 실제 구현(핀/HAL)은 없음.
- 예: `motor_if.h`가 `motor_drive(motion_cmd_t)`를 선언 →
  스테퍼 구현(`stepper_motor_if.c`)이 내부에서 `step_drive()` 호출.
- 효과: 모터 종류/핀이 바뀌어도 **구현 파일만** 교체, 기능 코드는 무수정.

```c
// UserDrivers/hal_if/motor_if.h  (계약)
typedef enum { MOTION_STOP, MOTION_FWD, MOTION_REV, MOTION_LEFT, MOTION_RIGHT } motion_cmd_t;
void motor_drive(motion_cmd_t cmd);
```

### ipc (Inter-Process Communication)
- 태스크끼리 **안전하게 데이터를 주고받는 통로**. 전역변수 난무 대체.
- FreeRTOS 수단: **큐 / 태스크 notification / 이벤트 플래그 / 뮤텍스**.
- `App/ipc/`에 큐 핸들·메시지 타입을 한 곳에 정의(계약).

```c
// App/ipc/msg_types.h
typedef struct { float yaw; uint8_t color_left, color_right; } sensor_msg_t;
extern osMessageQueueId_t sensor_queue;  // sensor_task -> control_task
extern osMessageQueueId_t cmd_queue;     // comms_task  -> control_task
```

### services vs tasks
- **services/**: 하드웨어 독립 로직 엔진(모션 조립, 센서퓨전, 캘리 로직, 프로토콜 파서).
- **tasks/**: RTOS 오케스트레이션만. 큐 받고 → service 호출 → 결과 발행. 얇게 유지.

---

## 3. 목표 폴더 구조

```
ROKI_U375_RTOS/
├── Core/                      # CubeMX: main.c, *_it.c, app_freertos.c
├── Drivers/  Middlewares/     # CubeMX: HAL, FreeRTOS
│
├── UserDrivers/               # ── 하드웨어 향함 ──
│   ├── common/                # def.h, utils, ring buffer, err codes
│   ├── bsp/
│   │   ├── board.h            # ★ 전체 핀맵/주변장치 매핑 (PCB 바뀌면 여기만)
│   │   ├── i2c/  uart/  swv/  # 버스 래퍼
│   │   └── tim/  gpio/        # 타이머/GPIO 래퍼
│   ├── hal_if/                # ★ 추상 인터페이스 헤더만
│   │   └── imu_if.h motor_if.h led_if.h color_if.h buzzer_if.h ...
│   ├── drivers/               # imu/ color/ led/ rgb/ buzzer/ input/ ir/ power/
│   ├── actuator/              # stepper/
│   └── components/            # flash/
│
└── App/                       # ── 응용 (HW 안 봄) ──
    ├── ipc/                   # ★ msg_types.h, 큐/이벤트 핸들 선언
    ├── services/              # 재사용 로직 엔진 (HW 독립)
    │   ├── motion_ctrl/       # 모션 명령 (step_drive 감쌈)
    │   ├── sensor_fusion/     # IMU 적분/퓨전
    │   ├── calib_engine/      # 컬러 캘리 로직
    │   ├── protocol/          # PC 파서 + 핸들러
    │   ├── ui_feedback/       # RGB/buzzer 피드백 로직
    │   ├── bootloader/        # 부트로더 진입/태그
    │   └── flash_store/       # 설정/keymap 저장 (flash 컴포넌트 사용)
    │
    ├── features/              # 기능/모드 (자주 바뀜, 얇게)
    │   ├── mode_button/  mode_card/  mode_line_tracing/
    │   ├── mode_adjust/  calib/  jig/
    │
    ├── tasks/                 # ★ RTOS 오케스트레이션만
    │   ├── control_task.c     # features의 service를 고정주기 실행 (모터/모드)
    │   ├── sensor_task.c      # IMU/컬러 읽기 → ipc 발행
    │   ├── comms_task.c       # PC 프로토콜 RX/TX (구 pc_task)
    │   ├── supervisor_task.c  # 하트비트 수집 + IWDG 킥
    │   └── ui_task.c          # LED/RGB/buzzer 출력 (구 led_task)
    │
    └── app/
        └── app_init.c         # 태스크·큐 생성, 부팅 시퀀스 (구 ap_init + app_rtos_init)
```

---

## 4. 현재 → 신규 매핑표 (전부)

### UserDrivers (하드웨어 향함 — 대부분 거의 그대로)
| 현재 | 새 위치 | 처리 |
|---|---|---|
| `UserDrivers/bsp/{i2c,uart,swv}` | `UserDrivers/bsp/` | 그대로 + `board.h`(핀맵) 신설 |
| `UserDrivers/common/{def.h,utils}` | `UserDrivers/common/` | 그대로 |
| `UserDrivers/drivers/{imu,color,led,rgb,buzzer,input,ir,power}` | `UserDrivers/drivers/` | 복사 + 핀부 bsp 추출 |
| `UserDrivers/actuator/stepper` | `UserDrivers/actuator/stepper` | 복사 (모터 드라이버) |
| `UserDrivers/components/flash` | `UserDrivers/components/flash` | 그대로 (저수준 flash R/W) |
| *(신설)* | `UserDrivers/hal_if/` | `imu_if/motor_if/led_if...` 인터페이스 헤더 |

### App (응용 — 재배치 + 로직/구동 분리)
| 현재 | 새 위치 | 비고 |
|---|---|---|
| `App/ap/{ap.c,ap_isr}` | **해체** → `App/app/app_init.c` + `tasks/` + `Core/*_it.c` | 슈퍼루프 제거 핵심 |
| `App/lt_prog` | `App/features/mode_line_tracing` | 구동호출→motion 서비스 경유 |
| `App/card_prog` | `App/features/mode_card` | 〃 |
| `App/btn_prog` | `App/features/mode_button` | 〃 |
| `App/adjust` | `App/features/mode_adjust` | 파라미터 조정 모드 |
| `App/calib/{calib.c,calib_prog.c}` | `App/features/calib` + `App/services/calib_engine` | 로직/모드 분리 |
| `App/motion/{btn_action,card_action,line_tracing}` | `App/services/motion_ctrl` | 모션 조립 로직 |
| `App/action/rgb_actions` | `App/services/ui_feedback` | RGB/피드백 로직 |
| `App/bootloader` | `App/services/bootloader` | 부트로더 진입/태그 규약 |
| `App/jig` | `App/features/jig` (또는 `tools/`) | 공장 검사 모드 |
| `App/pc_coding/protocol` | `App/services/protocol` | 파서 (HW 독립, 거의 그대로) |
| `App/pc_coding/handler` | `App/services/protocol` | 명령 디스패치 |
| `App/pc_coding/app` | `App/app/app_init.c` | 태스크/큐 생성부 흡수 |
| `App/pc_coding/task/{pc_task,led_task}` | `App/tasks/comms_task.c`, `ui_task.c` | 태스크 계층으로 |

---

## 5. 반드시 보존할 것 (놓치기 쉬움)

부트로더가 펌웨어를 인식하려면 아래 레이아웃을 **새 링커 스크립트에도 동일**하게 유지해야 한다.

- **커스텀 링커 섹션** (기존 `ap.c` 참조):
  - `firmware_tag`  → `.tag`        (`firmware_tag_t`, magic `0xA1B2C3D4`, tag_size 2048)
  - `firmware_version` → `.version` (`firmware_version_t`)
  - `FLASH_systemData` → `.system_data` (`system_table_t`, endCode `0x12345678`)
  - `FLASH_keyMapData` → `.keyMap_data` (`keyMap_table_t[20]`)
- **부트로더 / 펌웨어 태그 규약**: 부트로더를 재사용하면 플래시 주소 맵 일치 필요.
- **캘리브레이션 플래시 데이터 포맷**: 기존 저장 데이터 호환 필요 시 유지.
- **링커 스크립트**: `STM32U375RGTX_FLASH.ld` / `_RAM.ld` 의 영역 정의.

---

## 6. 단계별 이식 순서

1. **새 CubeMX 프로젝트 생성** (STM32U375RGTx, RTOS=CMSIS-OS2 v2).
   - 클럭/타이머/IWDG/USART3 DMA 등 기존 설정 참고.
   - 핀은 **현재 ROKI 핀맵으로 임시 시작** → 새 PCB 나오면 .ioc에서 재배치.
   - 커스텀 링커 섹션(4장→5장) 반영.
2. **레이어 골격 생성**: 위 폴더 구조대로 빈 파일/헤더 배치.
3. **`hal_if/` 인터페이스 정의** (헤더만): motor/imu/led/color/buzzer 등.
4. **`ipc/` 정의**: msg_types.h, 큐/notification 핸들.
5. **태스크 골격**: control/sensor/comms/supervisor/ui 빈 태스크 + `app_init.c`에서 생성.
   - supervisor_task: 각 태스크 하트비트 플래그 수집 후에만 `HAL_IWDG_Refresh()`.
6. **드라이버 복사 + 핀부 bsp 추출**: 드라이버를 `hal_if` 구현으로 감쌈.
7. **검증 알고리즘 이식** (HW 독립): IMU 수학/퓨전, `pc_protocol` 파서, 컬러 분류.
8. **services 이식**: motion_ctrl(step_drive 래핑), ui_feedback, calib_engine.
9. **features 이식**: 모드 로직을 옮기되 드라이버 직접 호출을 service/hal_if 경유로 전환.
10. **계측·검증**: 스택 high-water mark, malloc 실패 훅, 스택오버플로 훅(=2) 활성.

---

## 7. RTOS 설정 점검 항목 (기존 대비 개선)

기존 `FreeRTOSConfig.h` 상태 및 개선 포인트:
- Tick 1000Hz, Heap 32KB, 선점형, MaxPrio 56, 타이머 태스크 on. (유지)
- **`configENABLE_FPU = 0`** 인데 IMU가 float 다수 사용 → FPU 활성/컨텍스트 저장 검토.
- **`configCHECK_FOR_STACK_OVERFLOW` 미정의** → `2`로 설정 + 훅 구현.
- **`configUSE_MALLOC_FAILED_HOOK` 미정의** → 활성 + 훅 구현.
- 런타임 통계(`configGENERATE_RUN_TIME_STATS`) 켜서 태스크 부하 측정 권장.
- 태스크 우선순위 평면화 탈피: comms/sensor/control/supervisor에 명확한 우선순위 부여.

---

## 8. 미해결/추후 결정

- 새 프로젝트 경로: 별도 폴더(`ROKI_U375_RTOS`) vs 완전 다른 경로 — **결정 필요**.
- 부트로더 재사용 여부 및 플래시 맵 확정 — PCB 확정 후.
- 모터 타입(스테퍼 유지 여부) — 회로 확정 후 `motor_if` 구현 결정.
- 기능 변경 범위 — 확정되면 `features/` 내용 재설계.
