# G356 快速上手

这份说明按“先确认模块能出数据，再打开 MCU 示例工程”的顺序写。第一次使用建议照着做一遍。

## 1. 先用 UART 确认模块正常

G356 上电后默认会主动输出 UART 数据，不需要先发命令。

接线：

| G356 | USB 转串口 / MCU | 说明 |
| :--- | :--- | :--- |
| VIN | 5V（推荐） | 输入范围 3.3V~12V，带 -20V 防反接保护 |
| GND | GND | 必须共地 |
| TXD | RXD | G356 输出遥测数据 |
| RXD | TXD | 只有需要下发配置命令时才必须接 |

串口参数：`115200, 8N1, 无流控`。

最简单验证方法：

```powershell
cd E:\IMU\G356_example\PC_UART_Monitor
python g356_uart_monitor.py COM5
```

把 `COM5` 换成电脑里实际的串口号。正常时会看到类似：

```text
roll=0.12 pitch=-1.35 yaw=86.40 acc=[0.003,-0.012,1.001]g gyro=[0.02,-0.01,0.00]dps temp=28.4C fps=100.0
```

如果没有装 `pyserial`，先执行：

```powershell
python -m pip install pyserial
```

## 2. 推荐：直接复制 G356_SDK

客户已有自己的 MCU 工程时，推荐直接复制：

```text
G356_example/G356_SDK/
```

然后：

1. 打开 `g356_config.h`，选择 UART 或 SPI。
2. 把 `g356_port_template.c` 加到工程里，按注释实现底层 UART/SPI 函数。
3. 业务代码里调用 `G356_Init()` 和 `G356_Update()`。

最小业务代码：

```c
#include "g356.h"

static G356_Handle_t g356;

int main(void)
{
    board_init();
    G356_Init(&g356);

    while (1) {
        if (G356_Update(&g356) == G356_RESULT_OK) {
            // g356.data.roll / pitch / yaw / accel_x ... 直接使用
        }
    }
}
```

## 3. 需要完整例程时，再选择 MCU 示例工程

如果客户想直接打开一个能编译下载的完整工程，按手里的平台选目录：

- LP-MSPM0G3507：打开 `MSPM0G3507/UART/` 或 `MSPM0G3507/SPI/`
- MSPM0G3519：打开 `MSPM0G3519/UART/` 或 `MSPM0G3519/SPI/`
- STM32F103C8T6：打开 `STM32F103C8T6/UART/HAL/`、`STM32F103C8T6/SPI/HAL/` 或对应 `Standard/`
- STM32F407VET6：打开 `STM32F407VET6/UART/`、`STM32F407VET6/SPI/` 或对应 `Standard/`

每个工程目录都有自己的 `README.md`。CCS 工程用 `File -> Import -> CCS Projects` 导入；Keil 工程直接打开 `.uvprojx`；Cube/HAL 工程可以先用 CubeMX 检查 `.ioc`，再用 Keil 编译。

## 4. UART 和 SPI 怎么选

建议优先用 UART：

- 接线少。
- 模块主动输出，不需要主控定时拉取。
- 更适合快速验证和普通低速采集。

需要更高确定性或主控统一拉取时再用 SPI：

- 主控必须做 SPI Controller/Master。
- G356 是 SPI Peripheral/Slave。
- 每次 CS 拉低后必须连续读满 `56` 字节。
- 下行配置命令仍建议走 UART，SPI 示例只负责读取遥测帧。

## 5. 旧版 g356_driver.c/.h 移植方式

各平台目录里保留了旧版 `g356_driver.c/.h`，已有项目可以继续使用。新项目建议优先用 `G356_SDK/`。

旧版 UART 移植最小流程：

```c
uint8_t frame[G356_FRAME_SIZE];
G356_Data_t data;

void on_uart_rx_byte(uint8_t byte)
{
    if (G356_FeedByte(byte, frame) == G356_FRAME_VALID) {
        G356_ParseData(frame, &data);
        // data.roll / data.pitch / data.yaw / data.accel_x ...
    }
}
```

旧版 SPI 移植最小流程：

```c
uint8_t frame[G356_FRAME_SIZE];
G356_Data_t data;

if (G356_ReadPacket(frame)) {
    G356_ParseData(frame, &data);
}
```

注意：旧版 `G356_ReadPacket()` 里的 SPI 外设名、CS GPIO 名来自示例工程的 SysConfig/Cube 配置，换到自己的板子时需要改成自己的外设句柄和片选引脚。

## 6. 常见问题

看不到数据：

- 先确认 G356 供电在 `3.3V~12V` 范围内，推荐使用稳定 `5V`，并且和主控/USB 转串口共地。
- UART 先只接 `G356 TXD -> 接收端 RXD`，不要把 TX/RX 接反。
- 串口参数必须是 `115200, 8N1`，除非你已经改过模块波特率。
- 如果怀疑波特率被改乱，用上位机的一键恢复或按协议发送 `0x19` 恢复到 `115200`。

数据偶尔校验失败：

- UART：检查线长、地线、电源纹波和串口波特率。
- SPI：检查 CS 是否一次只包住完整 `56` 字节，SCLK 是否先从 `2 MHz` 起步，模式是否为 Mode 0。

姿态角看起来不动：

- 确认你解析的是 `0xAA 0x55` 的 G356 56 字节帧，不要误用 G357 的 16 字节帧格式。
- 确认上位机或代码没有只打印整数部分。

编译不过：

- CCS 工程优先检查 SDK、SysConfig 和 compiler 版本。
- Keil 工程优先检查芯片包和 include path。
- 自己项目里移植时，优先只集成 `G356_SDK/`，把演示工程里的打印函数和板级初始化留在原工程里。
