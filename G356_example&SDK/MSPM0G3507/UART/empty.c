#include <stdio.h>
#include "ti_msp_dl_config.h"
#include "g356_driver.h"

// ==========================================================================
// G356 UART Receiver Demo
//
// This file only demonstrates how to call the reusable G356 driver
// (g356_driver.h/.c). If you're integrating G356 into your own product,
// you only need to copy g356_driver.h/.c — the statistics/pretty-printing
// below are demo-only and can be deleted.
//
// Wiring and protocol details: see g356_driver.h and ../README.md.
// ==========================================================================

// ==========================================
// 延迟函数：基于毫秒的软件忙等待延时
// ==========================================
static void delay_ms(uint32_t ms)
{
    // CPUCLK_FREQ 在 ti_msp_dl_config.h 中定义为 32000000 (32MHz)
    // 32000 个时钟周期对应 1 毫秒的延时
    delay_cycles(ms * (CPUCLK_FREQ / 1000));
}

// ==========================================
// 串口重定向：支持使用标准 printf 打印调试信息
// ==========================================
#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
// TI Clang 编译器 printf 重定向底层支持
int fputc(int ch, FILE *f)
{
    // 阻塞式发送单个字符到调试串口 UART_DEBUG_INST
    DL_UART_transmitDataBlocking(UART_DEBUG_INST, (uint8_t)ch);
    return ch;
}

#endif

// ==========================================
// 调试串口输出辅助函数 (避免依赖大体积的 C 库 printf 及其浮点支持)
// ==========================================
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
    uint32_t int_part = (uint32_t)val;
    G356_UART_PrintUint(int_part);

    // 小数部分
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

// ==========================================
// 主函数入口
// ==========================================
int main(void)
{
    // 1. 系统及外设初始化 (包含时钟、UART_G356, UART_DEBUG)
    SYSCFG_DL_init();

    // 2. 延时 1 秒，等待 G356 模块内部系统彻底启动并稳定
    delay_ms(1000);

    // 3. 打印欢迎和指示信息
    G356_UART_PrintString("\r\n");
    G356_UART_PrintString("=========================================\r\n");
    G356_UART_PrintString("   G356 AHRS Module UART Receiver Demo   \r\n");
    G356_UART_PrintString("   MCU: MSPM0G3507 | Baudrate: 115200    \r\n");
    G356_UART_PrintString("   Communication: UART (115200, 8N1)     \r\n");
    G356_UART_PrintString("=========================================\r\n");
    G356_UART_PrintString("Starting data reading...\r\n");

    uint8_t rx_frame_buf[G356_FRAME_SIZE]; // 校验通过的完整帧缓冲区
    G356_Data_t g356_data;                 // 解析后的姿态数据结构体
    uint32_t valid_count = 0;              // 校验通过的帧计数
    uint32_t invalid_count = 0;            // 校验失败的帧计数

    while (1) {
        // G356 以约 100Hz 持续往外发送遥测帧，这里逐字节阻塞接收并喂给状态机
        // (UART 是连续字节流，没有 SPI 那种片选信号天然分帧，所以用状态机
        //  自己识别帧头、定长收集、校验)
        uint8_t byte = DL_UART_receiveDataBlocking(UART_G356_INST);
        G356_FrameStatus status = G356_FeedByte(byte, rx_frame_buf);

        if (status == G356_FRAME_VALID) {
            valid_count++;

            // 解析原始数据帧到浮点工程单位
            G356_ParseData(rx_frame_buf, &g356_data);

            // 每成功读取 20 次 (约 200ms) 打印一次数据，防止串口拥堵
            if (valid_count % 20 == 0) {
                // 打印姿态欧拉角 (Roll, Pitch, Yaw)
                G356_UART_PrintString("ANG[R:");
                G356_UART_PrintFloat(g356_data.roll, 2);
                G356_UART_PrintString(", P:");
                G356_UART_PrintFloat(g356_data.pitch, 2);
                G356_UART_PrintString(", Y:");
                G356_UART_PrintFloat(g356_data.yaw, 2);

                // 打印加速度计原始值 (g)
                G356_UART_PrintString("] deg | ACC[");
                G356_UART_PrintFloat(g356_data.accel_x, 3);
                G356_UART_PrintString(", ");
                G356_UART_PrintFloat(g356_data.accel_y, 3);
                G356_UART_PrintString(", ");
                G356_UART_PrintFloat(g356_data.accel_z, 3);

                // 打印陀螺仪角速度 (dps)
                G356_UART_PrintString("] g | GYR[");
                G356_UART_PrintFloat(g356_data.gyro_x, 1);
                G356_UART_PrintString(", ");
                G356_UART_PrintFloat(g356_data.gyro_y, 1);
                G356_UART_PrintString(", ");
                G356_UART_PrintFloat(g356_data.gyro_z, 1);

                // 打印温度
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
