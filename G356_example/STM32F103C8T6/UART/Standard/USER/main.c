#include "stm32f10x.h"
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "g356_driver.h"

// ==========================================================================
// G356 UART Receiver Demo (STM32F103C8T6, StdPeriph library)
//
// This file only demonstrates how to call the reusable G356 driver
// (HARDWARE/G356/g356_driver.h/.c). If you're integrating G356 into your
// own product, you only need to copy that one file pair -- the
// statistics/pretty-printing below are demo-only and can be deleted.
//
// Wiring and protocol details: see g356_driver.h and ../README.md.
// ==========================================================================

// ==========================================
// 调试串口输出辅助函数 (避免依赖大体积的 C 库 printf 及其浮点支持)
// ==========================================
static void G356_UART_PrintString(const char *str)
{
    while (*str) {
        uart1_send_byte((u8)*str++);
    }
}

static void G356_UART_PrintUint(u32 val)
{
    char buf[12];
    int i = 11;
    buf[i] = '\0';
    if (val == 0) {
        G356_UART_PrintString("0");
        return;
    }
    while (val > 0) {
        i--;
        buf[i] = (val % 10) + '0';
        val /= 10;
    }
    G356_UART_PrintString(&buf[i]);
}

static void G356_UART_PrintFloat(float val, int decimals)
{
    // 处理负数
    if (val < 0.0f) {
        G356_UART_PrintString("-");
        val = -val;
    }

    // 四舍五入处理
    float rounding = 0.5f;
    for (int d = 0; d < decimals; d++) {
        rounding /= 10.0f;
    }
    val += rounding;

    // 整数部分
    u32 int_part = (u32)val;
    G356_UART_PrintUint(int_part);

    // 小数部分
    if (decimals > 0) {
        G356_UART_PrintString(".");
        float frac = val - (float)int_part;
        for (int d = 0; d < decimals; d++) {
            frac *= 10.0f;
            u32 digit = (u32)frac;
            G356_UART_PrintUint(digit);
            frac -= (float)digit;
        }
    }
}

int main(void)
{
    u8 rx_frame_buf[G356_FRAME_SIZE]; // 校验通过的完整帧缓冲区
    G356_Data_t g356_data;            // 解析后的姿态数据结构体
    u32 valid_count = 0;              // 校验通过的帧计数
    u32 invalid_count = 0;            // 校验失败的帧计数

    // SystemInit() 已由启动文件在进入 main() 之前调用，按
    // system_stm32f10x.c 中 SYSCLK_FREQ_72MHz 的默认配置，把 72MHz
    // PLL 时钟（HSE 8MHz 倍频 9）配置好了，这里不需要再手动配置时钟。

    delay_init();
    NVIC_Configuration();
    uart1_init(115200);
    uart2_init(115200);

    delay_ms(1000); // 等待 G356 模块内部系统彻底启动并稳定

    G356_UART_PrintString("\r\n");
    G356_UART_PrintString("=========================================\r\n");
    G356_UART_PrintString("   G356 AHRS Module UART Receiver Demo   \r\n");
    G356_UART_PrintString("   MCU: STM32F103C8T6 | Baudrate: 115200 \r\n");
    G356_UART_PrintString("   Communication: USART2 (115200, 8N1)   \r\n");
    G356_UART_PrintString("=========================================\r\n");
    G356_UART_PrintString("Starting data reading...\r\n");

    while (1) {
        // G356 以约 100Hz 持续往外发送遥测帧，这里逐字节阻塞接收并喂给状态机
        // (UART 是连续字节流，没有 SPI 那种片选信号天然分帧，所以用状态机
        //  自己识别帧头、定长收集、校验)
        u8 byte = uart2_recv_byte();
        G356_FrameStatus status = G356_FeedByte(byte, rx_frame_buf);

        if (status == G356_FRAME_VALID) {
            valid_count++;

            G356_ParseData(rx_frame_buf, &g356_data);

            // 每成功读取 20 次 (约 200ms) 打印一次数据，防止串口拥堵
            if (valid_count % 20 == 0) {
                G356_UART_PrintString("ANG[R:");
                G356_UART_PrintFloat(g356_data.roll, 2);
                G356_UART_PrintString(", P:");
                G356_UART_PrintFloat(g356_data.pitch, 2);
                G356_UART_PrintString(", Y:");
                G356_UART_PrintFloat(g356_data.yaw, 2);

                G356_UART_PrintString("] deg | ACC[");
                G356_UART_PrintFloat(g356_data.accel_x, 3);
                G356_UART_PrintString(", ");
                G356_UART_PrintFloat(g356_data.accel_y, 3);
                G356_UART_PrintString(", ");
                G356_UART_PrintFloat(g356_data.accel_z, 3);

                G356_UART_PrintString("] g | GYR[");
                G356_UART_PrintFloat(g356_data.gyro_x, 1);
                G356_UART_PrintString(", ");
                G356_UART_PrintFloat(g356_data.gyro_y, 1);
                G356_UART_PrintString(", ");
                G356_UART_PrintFloat(g356_data.gyro_z, 1);

                G356_UART_PrintString("] dps | Temp:");
                G356_UART_PrintFloat(g356_data.temp, 1);
                G356_UART_PrintString(" C\r\n");
            }
        } else if (status == G356_FRAME_INVALID) {
            invalid_count++;

            // 每失败 100 次打印一次错误警告，方便排查接线/波特率问题
            if (invalid_count % 100 == 0) {
                G356_UART_PrintString("[Warning] Frame validation failed! Valid: ");
                G356_UART_PrintUint(valid_count);
                G356_UART_PrintString(", Invalid: ");
                G356_UART_PrintUint(invalid_count);
                G356_UART_PrintString("\r\n");
            }
        }
        // status == G356_FRAME_PENDING: 还没收完一整帧，继续读下一个字节
    }
}
