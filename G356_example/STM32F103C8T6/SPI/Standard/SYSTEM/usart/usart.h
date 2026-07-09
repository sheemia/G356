#ifndef __USART_H
#define __USART_H
#include "stm32f10x.h"

// USART1: 调试打印口 (PA9=TX, PA10=RX), 115200 8N1
void uart1_init(u32 baud);
void uart1_send_byte(u8 data);

#endif
