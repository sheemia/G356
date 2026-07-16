#include <stdio.h>
#include <stdbool.h>
#include "ti_msp_dl_config.h"
#include "g356_driver.h"

// ==========================================================================
// G356 UART Receiver Demo - interrupt driven version
//
// G356 continuously outputs UART telemetry. In real products, do not depend on
// a slow main loop or debug printing to drain the MCU UART FIFO. This demo uses
// UART RX interrupt to consume every byte, then the main loop only prints the
// latest parsed frame at a low rate.
// ==========================================================================

#define G356_UART_INT_MASK                                                   \
    (DL_UART_INTERRUPT_RX | DL_UART_INTERRUPT_RX_TIMEOUT_ERROR |             \
        DL_UART_INTERRUPT_OVERRUN_ERROR | DL_UART_INTERRUPT_BREAK_ERROR |    \
        DL_UART_INTERRUPT_PARITY_ERROR | DL_UART_INTERRUPT_FRAMING_ERROR)

static uint8_t g356_rx_frame_buf[G356_FRAME_SIZE];
static volatile G356_Data_t g356_latest_data;
static volatile uint32_t g356_valid_count;
static volatile uint32_t g356_invalid_count;
static volatile uint32_t g356_error_count;
static volatile bool g356_new_frame;

static void delay_ms(uint32_t ms)
{
    delay_cycles(ms * (CPUCLK_FREQ / 1000U));
}

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
int fputc(int ch, FILE *f)
{
    (void)f;
    DL_UART_transmitDataBlocking(UART_DEBUG_INST, (uint8_t)ch);
    return ch;
}
#endif

static void G356_UART_PrintString(const char *str)
{
    while (*str) {
        DL_UART_transmitDataBlocking(UART_DEBUG_INST, (uint8_t)*str++);
    }
}

static void G356_UART_PrintUint(uint32_t val)
{
    char buf[12];
    int i = 11;
    buf[i] = '\0';
    if (val == 0U) {
        G356_UART_PrintString("0");
        return;
    }
    while (val > 0U) {
        i--;
        buf[i] = (char)((val % 10U) + '0');
        val /= 10U;
    }
    G356_UART_PrintString(&buf[i]);
}

static void G356_UART_PrintFloat(float val, int decimals)
{
    if (val < 0.0f) {
        G356_UART_PrintString("-");
        val = -val;
    }

    float rounding = 0.5f;
    for (int d = 0; d < decimals; d++) {
        rounding /= 10.0f;
    }
    val += rounding;

    uint32_t int_part = (uint32_t)val;
    G356_UART_PrintUint(int_part);

    if (decimals > 0) {
        G356_UART_PrintString(".");
        float frac = val - (float)int_part;
        for (int d = 0; d < decimals; d++) {
            frac *= 10.0f;
            uint32_t digit = (uint32_t)frac;
            G356_UART_PrintUint(digit);
            frac -= (float)digit;
        }
    }
}

static void G356_ProcessRxData(uint32_t rxdata)
{
    uint32_t errors = rxdata & (DL_UART_ERROR_OVERRUN | DL_UART_ERROR_BREAK |
                                  DL_UART_ERROR_PARITY | DL_UART_ERROR_FRAMING);
    uint8_t byte = (uint8_t)(rxdata & UART_RXDATA_DATA_MASK);
    G356_FrameStatus status = G356_FeedByte(byte, g356_rx_frame_buf);

    if (errors != 0U) {
        g356_error_count++;
    }

    if (status == G356_FRAME_VALID) {
        G356_Data_t parsed;
        G356_ParseData(g356_rx_frame_buf, &parsed);
        g356_latest_data = parsed;
        g356_valid_count++;
        g356_new_frame = true;
    } else if (status == G356_FRAME_INVALID) {
        g356_invalid_count++;
    }
}

static void G356_DrainUartFifo(void)
{
    while (!DL_UART_isRXFIFOEmpty(UART_G356_INST)) {
        G356_ProcessRxData(UART_G356_INST->RXDATA);
    }
}

static void G356_StartUartRxInterrupt(void)
{
    DL_UART_setRXFIFOThreshold(UART_G356_INST, DL_UART_RX_FIFO_LEVEL_ONE_ENTRY);
    DL_UART_setRXInterruptTimeout(UART_G356_INST, 1U);
    DL_UART_clearInterruptStatus(UART_G356_INST, G356_UART_INT_MASK);
    G356_DrainUartFifo();
    DL_UART_enableInterrupt(UART_G356_INST, G356_UART_INT_MASK);
    NVIC_ClearPendingIRQ(UART_G356_INST_INT_IRQN);
    NVIC_EnableIRQ(UART_G356_INST_INT_IRQN);
}

void UART_G356_INST_IRQHandler(void)
{
    while (1) {
        switch (DL_UART_getPendingInterrupt(UART_G356_INST)) {
            case DL_UART_IIDX_RX:
            case DL_UART_IIDX_RX_TIMEOUT_ERROR:
                G356_DrainUartFifo();
                DL_UART_clearInterruptStatus(UART_G356_INST,
                    DL_UART_INTERRUPT_RX | DL_UART_INTERRUPT_RX_TIMEOUT_ERROR);
                break;

            case DL_UART_IIDX_OVERRUN_ERROR:
            case DL_UART_IIDX_BREAK_ERROR:
            case DL_UART_IIDX_PARITY_ERROR:
            case DL_UART_IIDX_FRAMING_ERROR:
                G356_DrainUartFifo();
                DL_UART_clearInterruptStatus(UART_G356_INST,
                    DL_UART_INTERRUPT_OVERRUN_ERROR | DL_UART_INTERRUPT_BREAK_ERROR |
                        DL_UART_INTERRUPT_PARITY_ERROR | DL_UART_INTERRUPT_FRAMING_ERROR);
                break;

            default:
                return;
        }
    }
}

static void G356_PrintFrame(const G356_Data_t *data, uint32_t valid,
    uint32_t invalid, uint32_t errors)
{
    G356_UART_PrintString("ANG[R:");
    G356_UART_PrintFloat(data->roll, 2);
    G356_UART_PrintString(", P:");
    G356_UART_PrintFloat(data->pitch, 2);
    G356_UART_PrintString(", Y:");
    G356_UART_PrintFloat(data->yaw, 2);

    G356_UART_PrintString("] deg | ACC[");
    G356_UART_PrintFloat(data->accel_x, 3);
    G356_UART_PrintString(", ");
    G356_UART_PrintFloat(data->accel_y, 3);
    G356_UART_PrintString(", ");
    G356_UART_PrintFloat(data->accel_z, 3);

    G356_UART_PrintString("] g | GYR[");
    G356_UART_PrintFloat(data->gyro_x, 1);
    G356_UART_PrintString(", ");
    G356_UART_PrintFloat(data->gyro_y, 1);
    G356_UART_PrintString(", ");
    G356_UART_PrintFloat(data->gyro_z, 1);

    G356_UART_PrintString("] dps | Temp:");
    G356_UART_PrintFloat(data->temp, 1);
    G356_UART_PrintString(" C | Valid:");
    G356_UART_PrintUint(valid);
    G356_UART_PrintString(" Invalid:");
    G356_UART_PrintUint(invalid);
    G356_UART_PrintString(" UartErr:");
    G356_UART_PrintUint(errors);
    G356_UART_PrintString("\r\n");
}

int main(void)
{
    SYSCFG_DL_init();
    delay_ms(1000U);

    G356_UART_PrintString("\r\n");
    G356_UART_PrintString("=========================================\r\n");
    G356_UART_PrintString("   G356 AHRS Module UART Receiver Demo   \r\n");
    G356_UART_PrintString("   MCU: MSPM0G3519 | Baudrate: 115200    \r\n");
    G356_UART_PrintString("   UART RX: interrupt driven              \r\n");
    G356_UART_PrintString("=========================================\r\n");
    G356_UART_PrintString("Starting data reading...\r\n");

    G356_StartUartRxInterrupt();
    __enable_irq();

    uint32_t last_printed_valid = 0U;

    while (1) {
        if (g356_new_frame && (g356_valid_count != last_printed_valid) &&
            ((g356_valid_count % 20U) == 0U)) {
            G356_Data_t snapshot;
            uint32_t valid;
            uint32_t invalid;
            uint32_t errors;

            __disable_irq();
            snapshot = g356_latest_data;
            valid = g356_valid_count;
            invalid = g356_invalid_count;
            errors = g356_error_count;
            g356_new_frame = false;
            __enable_irq();

            last_printed_valid = valid;
            G356_PrintFrame(&snapshot, valid, invalid, errors);
        }
    }
}
