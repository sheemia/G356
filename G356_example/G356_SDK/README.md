# G356 SDK

这是推荐给客户优先使用的“无脑移植版”G356 驱动。客户不需要先读完所有示例工程，只要把本目录复制进自己的工程，然后补上少量底层 UART/SPI 函数即可。

## 最快用法

1. 复制整个 `G356_SDK/` 到客户工程。
2. 打开 `g356_config.h`，选择 `G356_USE_UART` 或 `G356_USE_SPI`。
3. 复制 `g356_port_template.c` 到自己的工程，按注释实现 UART/SPI 底层函数。
4. UART 项目中，推荐用 RX 中断或 DMA 持续接收，并把每个字节喂给 `G356_FeedByte()`。

## UART 推荐接收方式

G356 UART 是连续输出流。真实产品、机器人、小车、云台、飞控等实时项目中，推荐用 UART RX 中断或 DMA 连续接收，主循环只读取最新解析结果。

```c
#include "g356.h"

static G356_Handle_t g356;
static volatile bool g356_new_data;

void uart_rx_isr_or_callback(uint8_t byte)
{
    if (G356_FeedByte(&g356, byte) == G356_RESULT_OK) {
        g356_new_data = true;
    }
}

int main(void)
{
    board_init();
    G356_Init(&g356);
    uart_start_rx_interrupt_or_dma();

    while (1) {
        if (g356_new_data) {
            g356_new_data = false;
            float roll = g356.data.roll;
            float pitch = g356.data.pitch;
            float yaw = g356.data.yaw;
            (void)roll;
            (void)pitch;
            (void)yaw;
        }
    }
}
```

## UART 轮询方式

`G356_Update()` 仍然保留，适合快速验证、PC 工具、Arduino 简单程序，或者从“软件环形缓冲区”里取字节：

```c
while (1) {
    if (G356_Update(&g356) == G356_RESULT_OK) {
        // g356.data 已更新
    }
}
```

注意：不要在低频主循环里直接轮询 MCU 硬件 UART FIFO。应用里如果还有显示刷新、printf、PID、无线通信等耗时任务，可能导致 UART FIFO overrun，表现为校验失败、丢帧或姿态跳变。

## 需要实现的底层函数

UART 轮询或环形缓冲区接收：

```c
int G356_PortUartReadByte(uint8_t *byte);
```

返回 `1` 表示读到一个字节，返回 `0` 表示当前没有字节，返回负数表示硬件错误。推荐让 UART ISR/DMA 先把数据放进 ring buffer，再由这个函数从 ring buffer 取数据。

如果要发送校准、归零、恢复出厂等下行命令，还要实现：

```c
int G356_PortUartWrite(const uint8_t *data, uint16_t len);
void G356_PortDelayMs(uint32_t ms);
```

SPI 读取遥测：

```c
int G356_PortSpiReadFrame(uint8_t *rx, uint16_t len);
```

这个函数内部要完成一次完整 SPI 事务：CS 拉低，连续读取 `len=56` 字节，CS 拉高。不要把同一帧拆成多次 CS 读取；模块会把每次 CS 低电平期间的输出视为一帧快照。

## 常用命令

```c
G356_SendCommandRepeated(G356_CMD_GYRO_CALIBRATE, 0);
G356_SendCommandRepeated(G356_CMD_ZERO_YAW, 0);
G356_SendCommandRepeated(G356_CMD_SET_OUTPUT_CHANNEL, G356_CHANNEL_UART);
G356_SendCommandRepeated(G356_CMD_SET_ODR_DIVIDER, G356_ODR_100HZ);
G356_SendCommandRepeated(G356_CMD_RECOVER_115200, 0);
```

`G356_SendCommandRepeated()` 默认发送两次，中间间隔 60ms，用来兼容部分 USB 转串口在空闲后第一包容易丢的问题。

## 数据字段

`G356_Handle_t.data` 里已经是工程单位：

- `accel_x/y/z`：单位 `g`
- `gyro_x/y/z`：单位 `dps`
- `roll/pitch/yaw`：单位 `deg`
- `temp`：单位 `C`
- `raw_accel_*` / `raw_gyro_*`：未量化、未扣校准 offset 的原始浮点六轴数据

## 移植建议

- 第一次调试优先选 UART，接线少，也更容易用 PC 串口工具排查。
- 真实控制项目中，UART 推荐中断或 DMA 接收，不推荐低频主循环直接轮询硬件 FIFO。
- SPI 模式必须一次 CS 周期内连续读满 56 字节，读完后再释放 CS。不要分段读取同一帧。
- 客户工程里只保留一份 `G356_SDK` 核心代码，不要从多个平台示例里混拷不同版本的 `g356_driver.c/.h`。
