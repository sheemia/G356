#include "g356_port.h"

/*
 * This file is a template. Copy it to your board support folder, rename it
 * if desired, and replace each TODO with your MCU vendor's UART/SPI APIs.
 *
 * UART receive recommendation:
 *   G356 outputs a continuous byte stream. For real products, use UART RX
 *   interrupt or DMA to drain the MCU UART FIFO continuously. A slow main-loop
 *   poll can overflow the hardware FIFO when your application is also doing
 *   display refresh, printf logging, control loops, wireless communication,
 *   etc.
 *
 * Recommended UART designs:
 *   1. UART ISR/DMA callback calls G356_FeedByte(&g356, byte) directly; or
 *   2. UART ISR/DMA callback writes bytes into a ring buffer, and
 *      G356_PortUartReadByte() reads from that ring buffer.
 *
 * Avoid printing debug logs inside the UART receive path.
 */

#if G356_USE_UART
int G356_PortUartReadByte(uint8_t *byte)
{
    (void)byte;

    /*
     * Polling integration pattern:
     *   if one byte is available from your software ring buffer:
     *       *byte = ring_buffer_pop();
     *       return 1;
     *   return 0;
     *
     * Do not use this function as a low-frequency direct hardware FIFO poll
     * in a busy application. Fill the ring buffer from UART RX interrupt or DMA.
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
     *   clock all len bytes continuously in this same CS-low transaction;
     *   for i in 0..len-1:
     *       rx[i] = spi_transfer(G356_SPI_DUMMY_BYTE);
     *   pull CS high;
     *
     * Do not split one 56-byte telemetry frame into multiple CS pulses.
     *   return 0;
     */
    return -1;
}
#endif