#include "g356_port.h"

/*
 * This file is a template. Copy it to your board support folder, rename it
 * if desired, and replace each TODO with your MCU vendor's UART/SPI APIs.
 */

#if G356_USE_UART
int G356_PortUartReadByte(uint8_t *byte)
{
    (void)byte;

    /*
     * TODO:
     *   if one byte is available:
     *       *byte = your_uart_read();
     *       return 1;
     *   return 0;
     */
    return 0;
}
#endif

#if G356_ENABLE_DOWNLINK
int G356_PortUartWrite(const uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;

    /*
     * TODO:
     *   write len bytes to the UART connected to G356 RXD.
     *   return 0 when all bytes are queued/sent.
     */
    return -1;
}

void G356_PortDelayMs(uint32_t ms)
{
    (void)ms;

    /*
     * TODO:
     *   delay for ms milliseconds.
     */
}
#endif

#if G356_USE_SPI
int G356_PortSpiReadFrame(uint8_t *rx, uint16_t len)
{
    (void)rx;
    (void)len;

    /*
     * TODO:
     *   pull CS low;
     *   for i in 0..len-1:
     *       rx[i] = spi_transfer(G356_SPI_DUMMY_BYTE);
     *   pull CS high;
     *   return 0;
     */
    return -1;
}
#endif
