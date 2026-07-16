#ifndef __SPI_H
#define __SPI_H

#include "stm32f4xx.h"
#include <stdint.h>

void SPI1_G356_Init(void);
uint8_t SPI1_G356_ReadPacket(uint8_t *buf, uint16_t len);

#endif
