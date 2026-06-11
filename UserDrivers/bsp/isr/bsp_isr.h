/*
 * bsp_isr.h
 *
 *  Created on: 2026. 6. 11.
 *      Author: RCY
 */

#ifndef BSP_BSP_ISR_H_
#define BSP_BSP_ISR_H_


void bsp_isr_tim_start(void);

void bsp_isr_tim4_callback(void);
void bsp_isr_tim6_callback(void);
void bsp_isr_tim7_callback(void);


#endif /* BSP_BSP_ISR_H_ */
