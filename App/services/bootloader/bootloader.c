/*
 * bootloader.c
 *
 *  Created on: 2026. 2. 9.
 *      Author: RCY
 */


#include "bootloader.h"


static void boot_write_bkp_data(uint32_t bkp_index, uint32_t data)
{
	uint32_t backup_register = 0U;

	/* Enable the write access to backup domain */
	SET_BIT(PWR->DBPR, PWR_DBPR_DBP);

	backup_register = (uint32_t)(TAMP_BASE + 0x100U);
	backup_register += (bkp_index * 4U);

	*(__IO uint32_t *) backup_register = data;

	/* Disable the write access to backup domain */
	CLEAR_BIT(PWR->DBPR, PWR_DBPR_DBP);
	NVIC_SystemReset();
}


void boot_enter_bootloader(void)
{
	uint32_t bkp_index = RTC_BKP_DR1;
	uint32_t flag      = 0x00000001U;

	boot_write_bkp_data(bkp_index, flag);
}


