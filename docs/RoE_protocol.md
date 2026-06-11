# RoE Unplugged UART Protocol 정의서

## 개요
- **Transport**: Bluetooth UART, 115200bps (roboroboBT SoftwareSerial → Firmata Stream)
- **프레임**: Firmata Sysex 기반
- **모든 데이터 바이트는 7-bit** (0x00~0x7F)

## 기본 프레임 구조

| 위치 | 값 | 설명 |
|---|---|---|
| Header | `0xF0` | START_SYSEX |
| ... | payload | 커맨드 + 데이터 |
| Tail | `0xF7` | END_SYSEX |

---

## 1. PC → 보드 (REQUEST)

모든 REQUEST: `F0 [command] [instruction] [args...] F7`
- `command = 0x00` → SET (쓰기)
- `command = 0x01` → GET (읽기)

---

### 1-1. SET (command=0x00)

#### 스텝모터 제어 (REQ_STEP_MOTOR = 0x05)

```
F0 00 05 [length] [step0] [value0] ([step1] [value1]) F7
```

- **length=2**: 모터 1개, **length=4**: 모터 2개

**value 비트필드:**

| 비트 | 마스크 | 설명 |
|---|---|---|
| bit6 | `0x40` | infinity (0=유한, 1=무한) |
| bit5 | `0x20` | speed (0=low, 1=high) |
| bit4 | `0x10` | direction (0=CW, 1=CCW) |
| bit3~2 | `0x0C` | motor select (0=left, 1=right) |
| bit1~0 | `0x03` | step MSB |

- **실제 스텝 수** = `(value & 0x03) << 7 | step_byte`

---

#### 출력 제어 (REQ_OUTPUT_W = 0x0A)

##### LED (argv[3]=0)
```
F0 00 0A 06 00 00 [R] [G] [B] [sign_bits] F7
```
- R, G, B: 하위 7비트 값
- sign_bits: `0x01`=R의 MSB, `0x02`=G의 MSB, `0x04`=B의 MSB
- 실제값: 예) `R_full = R | ((sign & 0x01) ? 0x80 : 0x00)`

##### Buzzer (argv[3]=1)
```
F0 00 0A 07 00 01 00 [freq_LSB] [freq_MSB] [dur_LSB] [dur_MSB] F7
```
- frequency = `freq_MSB << 7 | freq_LSB`
- duration = `dur_MSB << 7 | dur_LSB`

##### Sound (argv[3]=2)
```
F0 00 0A [length] 00 02 [soundNumber] F7
```
- 사전 정의된 사운드 번호로 재생

---

#### 모드 설정 (REQ_SET_MODE = 0x7D)

```
F0 00 7D 01 [mode] F7
```

| mode | 상수 | 설명 |
|---|---|---|
| `0x00` | COM_CODING_MODE | 코딩 모드 |
| `0x03` | COM_CARD_READING_MODE | 카드 리딩 모드 |
| `0x05` | COM_ALL_STOP_MODE | 전체 정지 (모터+LED+소리) |
| `0x06` | COM_CALIBRATION_MODE | 캘리브레이션 모드 |
| `0x08` | COM_LINETRACE_MODE | 라인트레이스 모드 |
| `0x11` | COM_UNPLUGGED_LED | 언플러그드-컬리(LED) |
| `0x12` | COM_UNPLUGGED_AUTOLED | 언플러그드-엘리(LED) |
| `0x13` | COM_UNPLUGGED_TOUCH | 언플러그드-이치(접촉센서) |
| `0x14` | COM_UNPLUGGED_MOTOR | 언플러그드-모티(MOTOR) |
| `0x15` | COM_UNPLUGGED_SPEAKER | 언플러그드-스피키(SPEAKER) |
| `0x16` | COM_UNPLUGGED_IR | 언플러그드-아리(IR) |
| `0x17` | COM_UNPLUGGED_LINETRACE_CURVE | 언플러그드-라이니 곡선 |
| `0x18` | COM_UNPLUGGED_LINETRACE_CROSS | 언플러그드-라이니 교차로 |

---

#### 캘리브레이션 (REQ_ADVANCED = 0x50)

##### 색상 설정
```
F0 00 50 [length] 03 [color_number] F7
```

##### 초기화
```
F0 00 50 [length] 04 F7
```

---

### 1-2. GET (command=0x01)

#### 버전 요청 (REQ_VERSION = 0x7F)
```
F0 01 7F F7
```

#### EEPROM 메모리 읽기 (REQ_MEMORY_ACCESS = 0x55)
```
F0 01 55 05 00 [startAddr_LSB] [startAddr_MSB] [endAddr_LSB] [endAddr_MSB] F7
```
- 주소 = `MSB << 7 | LSB`

---

## 2. 보드 → PC (RESPONSE)

### 2-1. sendRoboRoboResponseData 형식

```
F0 01 [instruction] [length] [data0] [data1] ... F7
```

#### 버전 정보 (RES_VERSION = 0x7F)
```
F0 01 7F 03 [model=0x40] [HW_VER] [FW_VER] F7
```

#### 센서 데이터 (RES_SENSOR = 0x02)

**컬러센서 (6바이트):**
```
F0 01 02 06 [LEFT_ID=0x00] [left_color] [0x00] [RIGHT_ID=0x01] [right_color] [0x00] F7
```

**버튼 (3바이트):**
```
F0 01 02 03 [SWITCH_ID=0x03] [button_value] [0x00] F7
```

**IR센서 (3바이트):**
```
F0 01 02 03 [IR_ID=0x02] [value] [0x00] F7
```
- value: 0=Black, 1=White

#### 캘리브레이션 raw 데이터 (RES_GET_COLOR = 0x03)
```
F0 01 03 06 [L_R%] [L_G%] [L_B%] [R_R%] [R_G%] [R_B%] F7
```

#### Advanced 응답 (RES_ADVANCED = 0x50)
```
F0 01 50 02 03 [response] F7
```

---

### 2-2. sendRoboRoboData 형식 (카드리딩 모드 전용)

```
F0 0C [category] [data] [checksum] F7
```
- **checksum** = `(~(category + data) + 1) & 0x7F`

| category | 상수 | 설명 |
|---|---|---|
| `0x31` | CATEGORY_ZERO_STEP | 색상 카드 인식 결과 |
| `0x7F` | CATEGORY_SPECIAL_CMD | 버튼 이벤트 |

**버튼 이벤트 data 값:**

| data | 설명 |
|---|---|
| `0x52` (R) | 클릭 |
| `0x44` (D) | 더블 클릭 |
| `0x53` (S) | 롱 클릭 |

---

### 2-3. EEPROM 응답 (직접 Serial.write)

```
F0 01 55 [length] 00 [LSB0] [MSB0] [LSB1] [MSB1] ... F7
```
- 각 EEPROM 바이트를 하위 4비트(LSB)와 상위 4비트(MSB)로 분리하여 전송

---

## 3. 참조 테이블

### Sensor ID

| ID | 상수 | 설명 |
|---|---|---|
| `0x00` | SENSOR_ID_LEFT | 왼쪽 컬러센서 |
| `0x01` | SENSOR_ID_RIGHT | 오른쪽 컬러센서 |
| `0x02` | SENSOR_ID_IR | IR 센서 |
| `0x03` | SENSOR_ID_SWITCH | 버튼 스위치 |

### Color 값

| 값 | 색상 |
|---|---|
| 0 | OFF |
| 1 | RED |
| 2 | ORANGE |
| 3 | YELLOW |
| 4 | GREEN |
| 5 | BLUE |
| 6 | PURPLE |
| 7 | YELLOW GREEN |
| 8 | SKY BLUE |
| 9 | PINK |
| 10 | BLACK |
| 11 | WHITE |

### Block Program Category

| 값 | 상수 | 설명 |
|---|---|---|
| `0x01` | CATEGORY_FUNC | 함수 |
| `0x04` | CATEGORY_ACT | 동작 |
| `0x08` | CATEGORY_SHAPE | 모양 |
| `0x0C` | CATEGORY_SOUND | 소리 |
| `0x10` | CATEGORY_PEN | 펜 |
| `0x14` | CATEGORY_DATA | 데이터 |
| `0x18` | CATEGORY_EVENT | 이벤트 |
| `0x1C` | CATEGORY_CONTROL | 제어 |
| `0x20` | CATEGORY_OBSERVE | 관찰 |
| `0x24` | CATEGORY_OPERATION | 연산 |
| `0x28` | CATEGORY_SPRITE | 스프라이트 |
| `0x2C` | CATEGORY_SOUND_SOURCE | 음원 |
| `0x30` | CATEGORY_QUIZ | 퀴즈 |
| `0x31` | CATEGORY_ZERO_STEP | 제로스텝 |
| `0x70` | CATEGORY_RODUINO | 로두이노 |
| `0x74` | CATEGORY_SCHOOL_KID | 스쿨키드 |
| `0x78` | CATEGORY_DRONE | 드론 |
| `0x7F` | CATEGORY_SPECIAL_CMD | 특수 명령 |

### HW/FW 버전

| 상수 | 값 | 설명 |
|---|---|---|
| ROBOROBO_HW_VERSION | `0x0A` (10) | V1.0 - TCS34725 컬러센서 |
| ROBOROBO_HW_VERSION__NEW_COLOR | `0x12` (18) | V1.8 - BH1749NUC 컬러센서 |
| ROBOROBO_FW_VERSION | `0x1C` (28) | 펌웨어 버전 |

---

## 4. 통신 흐름 요약

```
PC ──REQUEST──> 보드
  F0 00 ... F7   (SET: 모터/LED/버저/사운드/모드/캘리브레이션)
  F0 01 ... F7   (GET: 버전/EEPROM)

보드 ──RESPONSE──> PC
  F0 01 [inst] [len] [data...] F7   (센서/버전/캘리브레이션/Advanced)
  F0 0C [cat] [data] [csum] F7      (카드리딩 모드 블록 커맨드)
  F0 01 55 [len] 00 [nibbles...] F7  (EEPROM 데이터)
```
