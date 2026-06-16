/*
 * flash_store.c
 *
 *  Created on: 2026. 6. 16.
 *      Author: RCY
 */
#include "flash_store.h"


system_table_t RAM_systemData;
keyMap_table_t RAM_keyMapData[20];



extern uint32_t __flash_tag_addr;
extern uint32_t __isr_vector_addr;



const system_table_t FLASH_systemData __attribute__((section(".system_data"))) =
{
//	MOTION_write_start_addr
	FLASH_WRITE_ADDR_START,

//	MOTION_keymap_data 				-> Motion(CMD) Start Addr
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

/* HC Test(Modified by H.C PARK) ----------------------------------------- */
// MOTION_keymap_end_data			-> Motion(CMD) End Addr
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

	 // MOTION_keymap_size data		-> Motion(CMD) size
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

//	endCode
	0x12345678,
};

const keyMap_table_t FLASH_keyMapData[20] __attribute__((section(".keyMap_data"))) =
{
//	remocon_key, flag, main, start, end
	{0x00080000, 0x00, 0x64, 0xFF, 0xFF},	// Remocon R CMD = 100(STOP)
	{0x00040000, 0x00, 0x00, 0xFF, 0xFF},	// Remocon L CMD = 00
	{0x00001000, 0x00, 0x01, 0xFF, 0xFF},	// Remocon 1 CMD = 01
	{0x00004000, 0x00, 0x02, 0xFF, 0xFF},	// Remocon 2 CMD = 02
	{0x00008000, 0x00, 0x03, 0xFF, 0xFF},	// Remocon 3 CMD = 03
	{0x00002000, 0x00, 0x04, 0xFF, 0xFF},	// Remocon 4 CMD = 04
	{0x00100000, 0x00, 0x05, 0xFF, 0xFF},	// Remocon 5 CMD = 05
	{0x00400000, 0x00, 0x06, 0xFF, 0xFF},	// Remocon 6 CMD = 06
	{0x00800000, 0x00, 0x07, 0xFF, 0xFF},	// Remocon 7 CMD = 07
	{0x00200000, 0x00, 0x08, 0xFF, 0xFF},	// Remocon 8 CMD = 08
	{0x00FFFF00, 0x00, 0x00, 0xFF, 0xFF},	// Key Run	 CMD = 00
	{0x01FFFF00, 0x00, 0x01, 0xFF, 0xFF},	// Key 1 	 CMD = 01
	{0x02FFFF00, 0x00, 0x02, 0xFF, 0xFF},	// Key 2 	 CMD = 02
	{0x03FFFF00, 0x00, 0x03, 0xFF, 0xFF},	// Key 3 	 CMD = 03
	{0x04FFFF00, 0x00, 0x04, 0xFF, 0xFF},	// Key 4 	 CMD = 04
	{0xFFFFFFFF, 0x00, 0xFF, 0xFF, 0xFF},
	{0xFFFFFFFF, 0x00, 0xFF, 0xFF, 0xFF},
	{0xFFFFFFFF, 0x00, 0xFF, 0xFF, 0xFF},
	{0xFFFFFFFF, 0x00, 0xFF, 0xFF, 0xFF},
	{0xFFFFFFFF, 0x00, 0xFF, 0xFF, 0xFF}	// 19
};

__attribute__((section(".tag"))) firmware_tag_t firmware_tag =
{
	.number = 0xA1B2C3D4,

	// fw info
	.tag_addr = (uint32_t)&__flash_tag_addr,
	.fw_addr = (uint32_t)&__isr_vector_addr,
	.tag_size = 2048,
};

__attribute__((section(".version"))) firmware_version_t firmware_version =
{
	"ROKI - V1-dev-1",		// version
	"ROKI - Firmware"		// name
};



