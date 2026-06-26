/*
 * card_cmd.h
 *
 *  도메인 타입: 카드 모드에서 색상으로 해석되는 주행 명령.
 *  드라이버(센서)와 무관한 애플리케이션 공용 타입이므로 ipc 계층에 둔다.
 */

#ifndef IPC_CARD_CMD_H_
#define IPC_CARD_CMD_H_

#include "def.h"

/* 누적 가능한 최대 카드 명령 수 */
#define MAX_INSERTED_COMMANDS   20

typedef enum
{
    MODE_NONE = 0,
    MODE_FORWARD,
    MODE_BACKWARD,
    MODE_LEFT,
    MODE_RIGHT,
    MODE_REPEAT_ONCE,
    MODE_REPEAT_TWICE,
    MODE_REPEAT_THIRD,
	MODE_REPEAT_FOURTH,
    COLOR_MODE_COUNT
} color_mode_t;

#endif /* IPC_CARD_CMD_H_ */
