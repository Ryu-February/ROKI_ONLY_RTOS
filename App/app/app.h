/*
 * app.h
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#ifndef APP_APP_H_
#define APP_APP_H_


#include "utils.h"
#include "msg_types.h"

#include "ui_task.h"
#include "input_task.h"
#include "sensor_task.h"
#include "control_task.h"
#include "comms_task.h"
#include "supervisor_task.h"

typedef struct
{
	osThreadFunc_t 	func;
	const char 		*name;
	uint32_t 		stack_size;
	osPriority_t 	priority;
}task_init_t;

typedef struct
{
	uint32_t start_addr;
	uint32_t keyMap[20];

	/* HC Test(Modified by H.C PARK) ----------------------------------------- */
	uint32_t keymap_end_addr[20];
	uint16_t keymap_motion_size[20];
	/* ----------------------------------------------------------------------- */
	uint32_t endCode;

} system_table_t;


typedef struct
{
	uint32_t remocon_key;	// 4byte
	uint8_t start_end_flag;	// 1byte
	uint8_t main_CMD_key;	// 1byte
	uint8_t start_CMD_key;	// 1byte
	uint8_t end_CMD_key;	// 1byte

} keyMap_table_t;

typedef struct
{
	uint32_t number;

	// fw info
	uint32_t tag_addr;
	uint32_t fw_addr;
	uint32_t tag_size;

	// tag info
	uint32_t tag_flash_type;
	uint32_t tag_flash_start;
	uint32_t tag_flash_end;
	uint32_t tag_flash_length;
	uint32_t tag_flash_crc;
	uint32_t tag_length;
	uint8_t tag_date_str[32];
	uint8_t tag_time_str[32];

} firmware_tag_t;

typedef struct
{
	uint8_t version[32];
	uint8_t name[32];

} firmware_version_t;

#define FLASH_WRITE_ADDR_START						FLASH_MAINMEM_BANK2_START_ADDR
#define FLASH_MAINMEM_BANK2_START_ADDR				0x08080000U


void app_init(void);


#endif /* APP_APP_H_ */
