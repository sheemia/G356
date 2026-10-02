#include "spi.h"
#include "delay.h"
#include "g356_driver.h"

#define G356_CS_LOW()     GPIO_ResetBits(GPIOA, GPIO_Pin_4)
#define G356_CS_HIGH()    GPIO_SetBits(GPIOA, GPIO_Pin_4)

static uint8_t SPI1_ReadWriteByte(uint8_t tx)
{
    uint32_t timeout = 0x10000;

    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
        if (--timeout == 0) {
            return 0xFF;
        }
    }
    SPI_I2S_SendData(SPI1, tx);

    timeout = 0x10000;
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) {
        if (--timeout == 0) {
            return 0xFF;
        }
    }

    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
}

static uint8_t SPI1_WaitNotBusy(void)
{
    uint32_t timeout = 0x10000;

    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) != RESET) {
        if (--timeout == 0) {
            return 0;
        }
    }
    return 1;
}

void SPI1_G356_Init(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource5, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource6, GPIO_AF_SPI1);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_SPI1);

    gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_AF;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_OUT;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_OType = GPIO_OType_PP;
    gpio.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &gpio);
    G356_CS_HIGH();

    SPI_I2S_DeInit(SPI1);
    SPI_StructInit(&spi);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    /* APB2 = 84MHz. /64 = 1.3125MHz, safely within the 2MHz reference limit. */
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &spi);
    SPI_Cmd(SPI1, ENABLE);
}

uint8_t SPI1_G356_ReadPacket(uint8_t *buf, uint16_t len)
{
    uint16_t i;

    if (buf == 0 || len == 0) {
        return 0;
    }

    G356_CS_LOW();
    delay_us(100);
    for (i = 0; i < len; i++) {
        buf[i] = SPI1_ReadWriteByte(0xFF);
    }

    /* RXNE only confirms that the receive register contains the last byte.
     * Keep CS asserted until the peripheral has shifted the final clock out. */
    if (!SPI1_WaitNotBusy()) {
        G356_CS_HIGH();
        return 0;
    }
    G356_CS_HIGH();

    return 1;
}

uint8_t SPI1_G356_ReadFrame(uint8_t *buf, uint16_t *len)
{
    uint16_t i;
    uint16_t frame_size;

    if (buf == 0 || len == 0) {
        return 0;
    }

    G356_CS_LOW();
    delay_us(100);
    for (i = 0; i < 4u; ++i) {
        buf[i] = SPI1_ReadWriteByte(0xFF);
    }

    frame_size = (uint16_t)buf[3] + 2u;
    if (buf[0] != 0xAA || buf[1] != 0x55 ||
        !((frame_size == G356_COMMON_FRAME_SIZE && buf[2] == 0x04) ||
          (frame_size == G356_LEGACY_FRAME_SIZE && buf[2] == 0x02) ||
          (frame_size == G356_QUATERNION_FRAME_SIZE && buf[2] == 0x03))) {
        /* Keep CS low through the largest supported frame before retrying. */
        for (i = 4u; i < G356_MAX_FRAME_SIZE; ++i) {
            (void)SPI1_ReadWriteByte(0xFF);
        }
        (void)SPI1_WaitNotBusy();
        G356_CS_HIGH();
        return 0;
    }

    for (i = 4u; i < frame_size; ++i) {
        buf[i] = SPI1_ReadWriteByte(0xFF);
    }
    if (!SPI1_WaitNotBusy()) {
        G356_CS_HIGH();
        return 0;
    }
    G356_CS_HIGH();
    *len = frame_size;
    return 1;
}
