#ifndef __USART_H
#define __USART_H

#include "stm32f4xx.h"
#include <stdio.h>

void USART1_Debug_Init(uint32_t baud);
void USART2_G356_Init(uint32_t baud);
void USART1_PrintString(const char *str);
int USART2_ReadByte(uint8_t *byte);

#endif
