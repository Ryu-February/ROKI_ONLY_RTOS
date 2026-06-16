/*
 * bootloader.h
 *
 *  Created on: 2026. 2. 9.
 *      Author: RCY
 */

#ifndef BOOTLOADER_BOOTLOADER_H_
#define BOOTLOADER_BOOTLOADER_H_

#include "def.h"


#define BOOT_BKP_BOOT_FLAG_VALUE            0x00000001U

#define RTC_BKP_DR1                         0x00000001U
#define RTC_BKP_DR2                         0x00000002U
#define RTC_BKP_DR3                         0x00000003U
#define RTC_BKP_DR4                         0x00000004U
#define RTC_BKP_DR5                         0x00000005U
#define RTC_BKP_DR6                         0x00000006U
#define RTC_BKP_DR7                         0x00000007U
#define RTC_BKP_DR8                         0x00000008U
#define RTC_BKP_DR9                         0x00000009U
#define RTC_BKP_DR10                        0x0000000AU
#define RTC_BKP_DR11                        0x00000010U
#define RTC_BKP_DR12                        0x00000011U
#define RTC_BKP_DR13                        0x00000012U
#define RTC_BKP_DR14                        0x00000013U
#define RTC_BKP_DR15                        0x00000014U
#define RTC_BKP_DR16                        0x00000015U
#define RTC_BKP_DR17                        0x00000016U
#define RTC_BKP_DR18                        0x00000017U
#define RTC_BKP_DR19                        0x00000018U
#define RTC_BKP_DR20                        0x00000019U
#define RTC_BKP_DR21                        0x0000001AU
#define RTC_BKP_DR22                        0x0000001BU
#define RTC_BKP_DR23                        0x0000001CU
#define RTC_BKP_DR24                        0x0000001DU
#define RTC_BKP_DR25                        0x0000001EU
#define RTC_BKP_DR26                        0x0000001FU
#define RTC_BKP_DR27                        0x00000020U
#define RTC_BKP_DR28                        0x00000021U
#define RTC_BKP_DR29                        0x00000022U
#define RTC_BKP_DR30                        0x00000023U
#define RTC_BKP_DR31                        0x00000024U


void boot_enter_bootloader(void);
void bootloader_combo_tick(void);
bool bootloader_is_active(void);


#endif /* BOOTLOADER_BOOTLOADER_H_ */
