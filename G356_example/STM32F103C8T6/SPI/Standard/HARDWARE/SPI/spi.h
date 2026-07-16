#ifndef __SPI_H
#define __SPI_H
#include "stm32f10x.h"

// SPI2: G356 link (host = Controller). PB13=SCLK, PB14=MISO, PB15=MOSI.
// G356's own CS pin maps to PA2 here (software GPIO, not hardware NSS).
#define G356_CS_PORT    GPIOA
#define G356_CS_PIN     GPIO_Pin_2

#define G356_CS_LOW()   GPIO_ResetBits(G356_CS_PORT, G356_CS_PIN)
#define G356_CS_HIGH()  GPIO_SetBits(G356_CS_PORT, G356_CS_PIN)

void SPI2_Init(void);
u8 SPI2_ReadWriteByte(u8 tx_data);

#endif
