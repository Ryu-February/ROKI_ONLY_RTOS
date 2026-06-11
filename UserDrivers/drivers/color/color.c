/*
 * color.c
 *
 *  Created on: Sep 25, 2025
 *      Author: RCY
 */


#include "color.h"

#include "i2c.h"




/* --------- Low-level BH1749 --------- */

void bh1749_write_reg(uint8_t dev_addr, uint8_t reg, uint8_t data)
{
    i2c_write(dev_addr, reg, data);
}

uint8_t bh1749_read_u8(uint8_t dev_addr, uint8_t reg)
{
    return i2c_read(dev_addr, reg);
}

uint16_t bh1749_read_u16(uint8_t dev_addr, uint8_t lsb_reg)
{
    uint8_t lsb = i2c_read(dev_addr, lsb_reg);
    uint8_t msb = i2c_read(dev_addr, lsb_reg + 1);
    return (uint16_t)((msb << 8) | lsb);
}

void bh1749_init(uint8_t dev_addr, uint8_t rgb_gain, uint8_t ir_gain, uint8_t meas_mode)
{
    // 1) Software Reset
    bh1749_write_reg(dev_addr, BH1749_REG_SYSTEM_CONTROL, BH1749_SW_RESET);

    // 2) MODE_CONTROL1: [IR_GAIN][RGB_GAIN][MEAS_MODE]
    // 비트 배치는 데이터시트 정의에 따름. 기본값: x1/x1, 120ms
    uint8_t mode1 =
        ((ir_gain  & 0x03u) << 6) |
        ((rgb_gain & 0x03u) << 4) |
        (meas_mode & 0x07u);
    bh1749_write_reg(dev_addr, BH1749_REG_MODE_CTRL1, mode1);


    // 3) MODE_CONTROL2: RGB_EN=1 (측정 시작)
    bh1749_write_reg(dev_addr, BH1749_REG_MODE_CTRL2, BH1749_RGB_EN);

//    uint8_t data = i2c_read(BH1749_ADDR_LEFT, BH1749_REG_MODE_CTRL2);
    // (옵션) 유효 데이터 대기 (VALID 폴링)
    for (int i = 0; i < 5; i++)
    {
        uint8_t v = bh1749_read_u8(dev_addr, BH1749_REG_MODE_CTRL2);
        if (v & BH1749_VALID)
        {
            break;
        }
    }
}

/* --------- High-level Color --------- */

void color_init(void)
{
    // 기본: x1/x1, 120ms
    bh1749_init(BH1749_ADDR_LEFT,  BH1749_GAIN_X1,  BH1749_GAIN_X1,  BH1749_MEAS_35MS);
    bh1749_init(BH1749_ADDR_RIGHT, BH1749_GAIN_X1,  BH1749_GAIN_X1,  BH1749_MEAS_35MS);
}

bh1749_color_data_t bh1749_read_rgbir(uint8_t dev_addr)
{
    bh1749_color_data_t c;

    c.red   = bh1749_read_u16(dev_addr, BH1749_REG_RED_LSB);
    c.green = bh1749_read_u16(dev_addr, BH1749_REG_GREEN_LSB);
    c.blue  = bh1749_read_u16(dev_addr, BH1749_REG_BLUE_LSB);
    c.ir    = bh1749_read_u16(dev_addr, BH1749_REG_IR_LSB);

    return c;
}
