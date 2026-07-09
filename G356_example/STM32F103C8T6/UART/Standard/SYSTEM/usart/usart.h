#ifndef __USART_H
#define __USART_H
#include "stm32f10x.h"

// USART1: 调试打印口 (PA9=TX, PA10=RX), 115200 8N1
void uart1_init(u32 baud);
void uart1_send_byte(u8 data);

// USART2: G356 链路 (PA2=TX, PA3=RX), 115200 8N1 (需与 G356 当前配置波特率一致)
void uart2_init(u32 baud);
u8 uart2_recv_byte(void); // 阻塞等待接收一个字节

#endif
