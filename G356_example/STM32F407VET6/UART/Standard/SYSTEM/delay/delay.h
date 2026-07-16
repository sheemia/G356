#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f4xx.h"

void delay_init(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

#endif
