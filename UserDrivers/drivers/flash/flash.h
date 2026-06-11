/*
 * flash.h
 *
 *  Created on: Sep 9, 2025
 *      Author: RCY
 */

#ifndef COMPONENTS_FLASH_FLASH_H_
#define COMPONENTS_FLASH_FLASH_H_

#include "def.h"
#include "color.h"


#define FLASH_COLOR_TABLE_ADDR_LEFT   ((uint32_t)0x0803F800)               // Page 255
#define FLASH_COLOR_TABLE_ADDR_RIGHT  ((uint32_t)0x0803F000)               // Page 254 (하나 위)
#define FLASH_COLOR_ENTRY_SIZE        (sizeof(reference_entry_t))


void flash_write_color_reference(uint8_t sensor_side, uint8_t color_index, reference_entry_t entry);
reference_entry_t flash_read_color_reference(uint8_t sensor_side, uint8_t color_index);
void flash_erase_color_table(uint8_t sensor_side);


// ==== 스텝 인덱스 저장 영역 (Color 테이블 페이지 피해서) ====
// flash.h
#define FLASH_STEPPER_IDX_ADDR  	 ((uint32_t)0x0803E000)  // Bank2 빈 페이지 시작 정렬로

// 스텝 인덱스 저장/로드/지우기
void flash_write_step_index(uint16_t left_idx, uint16_t right_idx);
bool flash_read_step_index(uint16_t *out_left_idx, uint16_t *out_right_idx);
void flash_erase_step_index(void);

#define FLASH_CORR_STEPS_ADDR         ((uint32_t)0x0803D800)  // Bank2 빈 페이지 (FLASH_STEPPER_IDX_ADDR 이전 페이지)

// 보정 스텝 저장/로드/지우기 (좌회전, 우회전, 직진)
void flash_write_corr_steps(int16_t left_turn, int16_t right_turn, int16_t fwd);
bool flash_read_corr_steps(int16_t *out_left_turn, int16_t *out_right_turn, int16_t *out_fwd);
void flash_erase_corr_steps(void);

#endif /* COMPONENTS_FLASH_FLASH_H_ */
