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
	UI_EVT_RGB_TIMER_OFF,
	UI_EVT_COUNT
}ui_evt_type_t;

typedef struct
{
	ui_evt_type_t 	type;
	btn_id_t 		btn;
}ui_msg_t;


extern osMessageQueueId_t ui_queue;


#endif /* IPC_MSG_TYPES_H_ */
