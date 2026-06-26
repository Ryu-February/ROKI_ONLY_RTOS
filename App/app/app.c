/*
 * app.c
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */


#include "app.h"


const static task_init_t s_task_table[] =
{
		{ ui_task	, 		"ui_task", 			128 * 4, 	osPriorityLow 			},
		{ input_task, 		"input_task", 		128 * 4, 	osPriorityNormal 		},
		{ sensor_task, 		"sensor_task", 		512 * 4, 	osPriorityBelowNormal 	},
		{ control_task, 	"control_task", 	128 * 4, 	osPriorityAboveNormal	},
		{ comms_task, 		"comms_task", 		256 * 4, 	osPriorityAboveNormal	},
		{ supervisor_task, 	"supervisor_task", 	128 * 4, 	osPriorityLow			},
		{ calib_task,	 	"calib_task",	 	256 * 4, 	osPriorityNormal		},
};


#define NUM_STACKS (sizeof(s_task_table) / sizeof(s_task_table[0]))


osMessageQueueId_t ui_queue;
osMessageQueueId_t ctrl_queue;
osMessageQueueId_t rx_queue;
osMessageQueueId_t spvr_queue;
osMessageQueueId_t calib_queue;
osMessageQueueId_t sensor_queue;

void app_init(void)
{
	ui_queue = osMessageQueueNew(16, sizeof(ui_msg_t), NULL);
	ctrl_queue = osMessageQueueNew(8, sizeof(ctrl_msg_t), NULL);
	rx_queue = osMessageQueueNew(256, sizeof(uint8_t), NULL);	/* PC 프로토콜 RX 바이트 스트림 */
	spvr_queue = osMessageQueueNew(8, sizeof(spvr_msg_t), NULL);
	calib_queue = osMessageQueueNew(8, sizeof(calib_msg_t), NULL);
	sensor_queue = osMessageQueueNew(4, sizeof(sensor_msg_t), NULL);

	for (uint32_t i = 0; i < NUM_STACKS; i++)
	{
		osThreadAttr_t attr =
		{
				.name 		= s_task_table[i].name,
				.stack_size = s_task_table[i].stack_size,
				.priority 	= s_task_table[i].priority,
		};

		osThreadNew(s_task_table[i].func, NULL, &attr);
	}
}
