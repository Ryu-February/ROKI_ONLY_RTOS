/*
 * flash.c
 *
 *  Created on: Sep 9, 2025
 *      Author: RCY
 */


#include "flash.h"


void flash_write_color_reference(uint8_t sensor_side, uint8_t color_index, reference_entry_t entry)
{
    uint32_t base_addr = (sensor_side == BH1749_ADDR_LEFT) ? FLASH_COLOR_TABLE_ADDR_LEFT : FLASH_COLOR_TABLE_ADDR_RIGHT;
    uint32_t addr = base_addr + color_index * FLASH_COLOR_ENTRY_SIZE;

    HAL_FLASH_Unlock();

    // entry의 RAM 주소를 8바이트씩 건네준다 (포인터)
    for (size_t i = 0; i < sizeof(entry) / 8; i++)
    {
        uint32_t src_ptr = (uint32_t)(uintptr_t)((uint8_t*)&entry + i * 8);  // ✅ 포인터 전달
        HAL_StatusTypeDef st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i * 8, src_ptr);
        if (st != HAL_OK) break;
    }

    HAL_FLASH_Lock();
}

reference_entry_t flash_read_color_reference(uint8_t sensor_side, uint8_t color_index)
{
    uint32_t base_addr = (sensor_side == BH1749_ADDR_LEFT) ? FLASH_COLOR_TABLE_ADDR_LEFT : FLASH_COLOR_TABLE_ADDR_RIGHT;
    uint32_t addr = base_addr + color_index * FLASH_COLOR_ENTRY_SIZE;

    reference_entry_t entry;
    memcpy(&entry, (void*)addr, FLASH_COLOR_ENTRY_SIZE);

    return entry;
}

static inline void u375_get_bank_page(uint32_t addr, uint32_t *bank, uint32_t *page)
{
    const uint32_t SPLIT = 0x08080000UL;      // U375 고정 분기
    if (addr < SPLIT) {
        *bank = FLASH_BANK_1;
        *page = (addr - 0x08000000UL) / FLASH_PAGE_SIZE;
    } else {
        *bank = FLASH_BANK_2;
        *page = (addr - SPLIT) / FLASH_PAGE_SIZE;
    }
}

void flash_erase_color_table(uint8_t sensor_side)
{
    uint32_t base = (sensor_side == BH1749_ADDR_LEFT)
                  ? FLASH_COLOR_TABLE_ADDR_LEFT
                  : FLASH_COLOR_TABLE_ADDR_RIGHT;

    uint32_t bank, page;
    u375_get_bank_page(base, &bank, &page);

    FLASH_EraseInitTypeDef ei = {0};
    uint32_t page_error;

    ei.TypeErase = FLASH_TYPEERASE_PAGES;
    ei.Banks     = bank;     // ★ 반드시 지정
    ei.Page      = page;     // ★ 해당 뱅크 기준 페이지
    ei.NbPages   = 1;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    HAL_FLASHEx_Erase(&ei, &page_error);
    HAL_FLASH_Lock();
}



#define FLASH_STEPPER_IDX_MAGIC  (0xA55A5AA5UL)


typedef struct __attribute__((packed,aligned(8)))
{
    uint16_t left_idx;
    uint16_t right_idx;
    uint32_t magic;   // 유효성 확인용
} step_idx_blob_t;     // 총 8바이트 (더블워드 1개)

// --- 스텝 인덱스 페이지 Erase ---
void flash_erase_step_index(void)
{
    uint32_t bank, page;
    u375_get_bank_page(FLASH_STEPPER_IDX_ADDR, &bank, &page);

    FLASH_EraseInitTypeDef ei = {0};
    uint32_t page_error = 0;

    ei.TypeErase = FLASH_TYPEERASE_PAGES;
    ei.Banks     = bank;
    ei.Page      = page;
    ei.NbPages   = 1;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    (void)HAL_FLASHEx_Erase(&ei, &page_error);
    HAL_FLASH_Lock();
}

// --- 스텝 인덱스 저장 (더블워드 1회) ---
//  * 페이지가 지워진(0xFF) 상태여야 덮어쓰기 성공.
//  * 자주 쓰지 말고, 정지 시점 등 드물게 호출 권장.
void flash_write_step_index(uint16_t left_idx, uint16_t right_idx)
{
    step_idx_blob_t blob;
    blob.left_idx  = left_idx;
    blob.right_idx = right_idx;
    blob.magic     = FLASH_STEPPER_IDX_MAGIC;

//    1) 틀린 방식
//    uint64_t dw = 0;
//    memcpy(&dw, &blob, sizeof(blob));    // 8바이트 packing
//    HAL_FLASH_Unlock();
//    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
//    (void)HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
//                            FLASH_STEPPER_IDX_ADDR,
//                            dw);
//    HAL_FLASH_Lock();


//	2) src_ptr 처럼 하면 프로그램이 안 죽음
//    1번처럼 하면 프로그램 그대로 뻗어버림 (그냥 값을 복사해서 넘긴거라)
    //주소를 넘겨야 함 값이 아니라
    uint32_t src_ptr = (uint32_t)(uintptr_t)((uint8_t*)&blob);  // ✅ 포인터 전달
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    (void)HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                            FLASH_STEPPER_IDX_ADDR,
                            src_ptr);
    HAL_FLASH_Lock();
}

// --- 스텝 인덱스 로드 ---
bool flash_read_step_index(uint16_t *out_left_idx, uint16_t *out_right_idx)
{
    if (!out_left_idx || !out_right_idx)
        return false;

    step_idx_blob_t blob;
    memcpy(&blob, (const void*)FLASH_STEPPER_IDX_ADDR, sizeof(blob));

    if (blob.magic != FLASH_STEPPER_IDX_MAGIC)
        return false;

    *out_left_idx  = blob.left_idx;
    *out_right_idx = blob.right_idx;
    return true;
}

// ==== Correction steps (left_turn / right_turn / fwd) persistency ====
#define FLASH_CORR_STEPS_MAGIC16  (0xC33CU)

typedef struct __attribute__((packed,aligned(8)))
{
    int16_t  left_turn_corr;
    int16_t  right_turn_corr;
    int16_t  fwd_corr;
    uint16_t magic;
} corr_steps_blob_t;

void flash_erase_corr_steps(void)
{
    uint32_t bank, page;
    u375_get_bank_page(FLASH_CORR_STEPS_ADDR, &bank, &page);

    FLASH_EraseInitTypeDef ei = {0};
    uint32_t page_error = 0;

    ei.TypeErase = FLASH_TYPEERASE_PAGES;
    ei.Banks     = bank;
    ei.Page      = page;
    ei.NbPages   = 1;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    (void)HAL_FLASHEx_Erase(&ei, &page_error);
    HAL_FLASH_Lock();
}

void flash_write_corr_steps(int16_t left_turn, int16_t right_turn, int16_t fwd)
{
    corr_steps_blob_t blob;
    blob.left_turn_corr  = left_turn;
    blob.right_turn_corr = right_turn;
    blob.fwd_corr        = fwd;
    blob.magic           = FLASH_CORR_STEPS_MAGIC16;

    uint32_t src_ptr = (uint32_t)(uintptr_t)((uint8_t*)&blob);
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    (void)HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                            FLASH_CORR_STEPS_ADDR,
                            src_ptr);
    HAL_FLASH_Lock();
}

bool flash_read_corr_steps(int16_t *out_left_turn, int16_t *out_right_turn, int16_t *out_fwd)
{
    if (!out_left_turn || !out_right_turn || !out_fwd)
        return false;

    corr_steps_blob_t blob;
    memcpy(&blob, (const void*)FLASH_CORR_STEPS_ADDR, sizeof(blob));

    if (blob.magic != FLASH_CORR_STEPS_MAGIC16)
        return false;

    *out_left_turn  = blob.left_turn_corr;
    *out_right_turn = blob.right_turn_corr;
    *out_fwd        = blob.fwd_corr;
    return true;
}
