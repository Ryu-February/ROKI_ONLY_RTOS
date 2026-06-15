/*
 * msg_types.h
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#ifndef IPC_MSG_TYPES_H_
#define IPC_MSG_TYPES_H_


#include "btn.h"

typedef enum
{
	UI_EVT_BTN_PRESSED,
	UI_EVT_BAT_INDICATE,
	UI_EVT_RGB_TIMEOUT,
	UI_EVT_IR_DETECTED,
	UI_EVT_COUNT
}ui_evt_type_t;

typedef struct
{
	ui_evt_type_t 	type;
	btn_id_t 		btn;
	uint16_t		vbat;
	bool			ir_detected;
	bool			bat_low;
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

extern osMessageQueueId_t ui_queue;
extern osMessageQueueId_t ctrl_queue;


#endif /* IPC_MSG_TYPES_H_ */
