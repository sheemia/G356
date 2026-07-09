#include "stm32f10x.h"
#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "spi.h"
#include "g356_driver.h"

// ==========================================================================
// G356 SPI Receiver Demo (STM32F103C8T6, StdPeriph library)
//
// This file only demonstrates how to call the reusable G356 driver
// (HARDWARE/G356/g356_driver.h/.c, plus the SPI helper it depends on in
// HARDWARE/SPI/spi.h/.c). If you're integrating G356 into your own product,
// you only need to copy those two folders -- the retry loop, success/fail
// statistics, and pretty-printing below are demo-only and can be deleted.
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

static void G356_UART_PrintHex8(u8 val)
{
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[3];
    buf[0] = hex_chars[(val >> 4) & 0x0F];
    buf[1] = hex_chars[val & 0x0F];
    buf[2] = '\0';
    G356_UART_PrintString(buf);
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
    u8 rx_raw_buf[G356_FRAME_SIZE]; // 原始接收缓冲区
    G356_Data_t g356_data;          // 解析后的姿态数据结构体
    u32 success_count = 0;          // 成功包计数
    u32 fail_count = 0;             // 失败包计数

    // SystemInit() 已由启动文件在进入 main() 之前调用，按
    // system_stm32f10x.c 中 SYSCLK_FREQ_72MHz 的默认配置，把 72MHz
    // PLL 时钟（HSE 8MHz 倍频 9）配置好了，这里不需要再手动配置时钟。

    delay_init();
    NVIC_Configuration();
    uart1_init(115200);
    SPI2_Init();

    delay_ms(1000); // 等待 G356 模块内部系统彻底启动并稳定

    G356_UART_PrintString("\r\n");
    G356_UART_PrintString("=========================================\r\n");
    G356_UART_PrintString("   G356 AHRS Module SPI Receiver Demo    \r\n");
    G356_UART_PrintString("   MCU: STM32F103C8T6 | Baudrate: 115200 \r\n");
    G356_UART_PrintString("   Communication: SPI2 (Mode 0, ~2MHz)   \r\n");
    G356_UART_PrintString("=========================================\r\n");
    G356_UART_PrintString("Starting data reading...\r\n");

    while (1) {
        delay_ms(10); // 实现约 100Hz 的轮询读取速率

        // 尝试从 SPI 读取一个 56 字节数据包 (带 2 次立即重试，消除主从机异步复位碰撞)
        u8 read_ok = 0;
        for (int retry = 0; retry < 3; retry++) {
            if (G356_ReadPacket(rx_raw_buf)) {
                read_ok = 1;
                break;
            }
            delay_ms(1); // 每次失败后等待从机复位/预装载完毕再试
        }

        if (read_ok) {
            success_count++;

            G356_ParseData(rx_raw_buf, &g356_data);

            // 每成功读取 20 次 (约 200ms) 打印一次数据，防止串口拥堵
            if (success_count % 20 == 0) {
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
        } else {
            fail_count++;

            // 每失败 100 次打印一次错误警告和最近一次读取到的前 4 字节数据，方便排查接线或模式问题
            if (fail_count % 100 == 0) {
                G356_UART_PrintString("[Warning] SPI read failed! Success: ");
                G356_UART_PrintUint(success_count);
                G356_UART_PrintString(", Fail: ");
                G356_UART_PrintUint(fail_count);
                G356_UART_PrintString(". Raw: [0x");
                G356_UART_PrintHex8(rx_raw_buf[0]);
                G356_UART_PrintString(", 0x");
                G356_UART_PrintHex8(rx_raw_buf[1]);
                G356_UART_PrintString(", 0x");
                G356_UART_PrintHex8(rx_raw_buf[2]);
                G356_UART_PrintString(", 0x");
                G356_UART_PrintHex8(rx_raw_buf[3]);
                G356_UART_PrintString("]\r\n");
            }
        }
    }
}
