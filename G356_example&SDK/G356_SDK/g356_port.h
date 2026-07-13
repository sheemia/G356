#ifndef G356_PORT_H
#define G356_PORT_H

#include <stdint.h>

#include "g356_config.h"

/*
 * Implement these functions in your own board support file.
 * Copy g356_port_template.c as a starting point.
 */

#if G356_USE_UART
/*
 * Read one byte from the UART connected to G356 TXD.
 *
 * Return:
 *   1: one byte was read and stored in *byte
 *   0: no byte available right now
 *  <0: hardware/driver error
 */
int G356_PortUartReadByte(uint8_t *byte);
#endif

#if G356_ENABLE_DOWNLINK
/*
 * Write bytes to the UART connected to G356 RXD.
 *
 * Return:
 *   0: success
 *  <0: hardware/driver error
 */
int G356_PortUartWrite(const uint8_t *data, uint16_t len);

void G356_PortDelayMs(uint32_t ms);
#endif

#if G356_USE_SPI
/*
 * Read one complete SPI telemetry frame.
 *
 * The port function should:
 *   1. Pull CS low.
 *   2. Clock out exactly len bytes, usually by sending dummy 0xFF.
 *   3. Pull CS high.
 *
 * Return:
 *   0: success
 *  <0: hardware/driver error
 */
int G356_PortSpiReadFrame(uint8_t *rx, uint16_t len);
#endif

#endif /* G356_PORT_H */
