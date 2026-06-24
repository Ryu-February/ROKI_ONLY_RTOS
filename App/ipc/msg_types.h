/*
 * msg_types.h
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#ifndef IPC_MSG_TYPES_H_
#define IPC_MSG_TYPES_H_


#include "btn.h"
#include "card_cmd.h"

typedef enum
{
	UI_EVT_BTN_PRESSED,
	UI_EVT_BAT_INDICATE,
	UI_EVT_RGB_TIMEOUT,
	UI_EVT_IR_DETECTED,
	UI_EVT_CARD_INSERTED,
	UI_EVT_STBY_ENTERED,
	UI_EVT_STBY_TIMEOUT,
	UI_EVT_BOOT_ENTERED,
	UI_EVT_COUNT
}ui_evt_type_t;

typedef struct
{
	ui_evt_type_t 	type;
	btn_id_t 		btn;
	uint16_t		vbat;
	bool			ir_detected;
	bool			bat_low;
	color_mode_t	card;		// UI_EVT_CARD_INSERTED 시 확정된 명령
	uint8_t			card_count;	// 누적 카드 개수
	bool			stby_entered;
}ui_msg_t;

typedef enum
{
	SENS_IR,
	SENS_BAT
}sensor_kind_t;

typedef struct
{
	sensor_kind_t kind;
	union
	{
		bool ir_detected;
		bool bat_low;
	};
}sensor_evt_t;


typedef struct
{
	bool			ir_detected;
}ctrl_msg_t;

typedef enum
{
	SPVR_EVT_IDLE,
	SPVR_EVT_STBY_IDLE,
	SPVR_EVT_STBY_ENTERED,
	SPVR_EVT_STBY_TIMEOUT,
	SPVR_EVT_BOOTLOADER_IDLE,
	SPVR_EVT_BOOTLOADER_ENTERED,
	SPVR_EVT_COUNT,
}spvr_evt_type_t;

typedef struct
{
	spvr_evt_type_t type;
}spvr_msg_t;

extern osMessageQueueId_t ui_queue;
extern osMessageQueueId_t ctrl_queue;
extern osMessageQueueId_t rx_queue;
extern osMessageQueueId_t spvr_queue;
extern osMessageQueueId_t stby_queue;

#endif /* IPC_MSG_TYPES_H_ */
