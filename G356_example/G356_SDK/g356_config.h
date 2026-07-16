#ifndef G356_CONFIG_H
#define G356_CONFIG_H

/*
 * G356 SDK user configuration.
 *
 * Typical use:
 *   1. Copy the whole G356_SDK folder into your project.
 *   2. Select UART or SPI below.
 *   3. Implement the required functions declared in g356_port.h.
 */

#define G356_USE_UART                 1
#define G356_USE_SPI                  0

/* Enable helpers for 7-byte downlink commands such as calibration/reset. */
#define G356_ENABLE_DOWNLINK          1

/* Host-side workaround: send the same command twice with a short gap. */
#define G356_COMMAND_REPEAT_COUNT     2
#define G356_COMMAND_REPEAT_DELAY_MS  60

/* UART byte-stream parser behavior. */
#define G356_UART_MAX_BYTES_PER_UPDATE 128

/* SPI read behavior. The port layer should read one complete 56-byte frame. */
#define G356_SPI_DUMMY_BYTE           0xFF

#endif /* G356_CONFIG_H */
