# G356 示例工程入口

这个目录是 G356 六轴 AHRS 模块的客户示例包，目标是让用户不用先理解整套仓库，也能快速完成三件事：

1. 用 PC 串口工具确认 G356 正常输出数据。
2. 复制 `G356_SDK/` 到自己的工程里，按模板补 UART/SPI 底层函数。
3. 需要参考完整工程时，再选择对应 MCU 平台示例编译下载。

## 本次交付说明

G356 新设备和恢复出厂后的默认输出是48字节常用扩展帧 `AA 55 04 2E ... 5A`。旧设备升级时，Flash 中已有的有效格式设置继续生效；更早的固件若未存储帧格式标记，则继续输出原来的56字节兼容帧。56字节旧格式、72字节完整扩展、22字节仅四元数和自定义格式仍可通过命令选择。

| 平台 | SPI 示例实际时钟 | 片选方式 |
| :--- | :--- | :--- |
| MSPM0G3507 / MSPM0G3519 | 2MHz | PA27 软件片选；SCLK=PA17、MOSI=PB15、MISO=PA16（MOTO3） |
| STM32F103C8T6 | 2MHz | PA2 软件片选，SPI2（PB13/PB14/PB15） |
| STM32F407VET6 | 1.3125MHz | PA4 软件片选，SPI1（PA5/PA6/PA7） |

> SPI 频率建议从 2MHz 或更低开始；不要使用 PA18 作为 MSPM0G3519 的本示例 SPI 引脚。

## 先看哪一个文件

- `QUICK_START.md`：客户第一次使用请先看这里，包含接线、验证、工程选择和常见问题。
- `G356_SDK/`：推荐优先使用的无脑移植版驱动，核心协议代码只有一份。
- `PC_UART_Monitor/`：电脑端 UART 快速验证脚本，适合先确认模块、线序、波特率都没问题。
- 各平台子目录下的 `README.md`：对应工程的详细接线、导入、编译和移植说明。

## 推荐集成方式

客户自己的项目优先直接复制：

```text
G356_SDK/
  g356.h
  g356.c
  g356_config.h
  g356_port.h
  g356_port_template.c
```

只需要改两处：

1. 在 `g356_config.h` 选择 `G356_USE_UART` 或 `G356_USE_SPI`。
2. 按 `g356_port_template.c` 实现自己的 UART/SPI 收发函数。

UART 真实项目推荐使用中断或 DMA 接收。业务代码建议保持这种结构：

```c
#include "g356.h"

static G356_Handle_t g356;

int main(void)
{
    board_init();
    G356_Init(&g356);

    uart_start_rx_interrupt_or_dma();

    while (1) {
        if (g356_new_data) {
            g356_new_data = false;
            // g356.data.roll / pitch / yaw / accel_x ... 直接使用
        }
    }
}
```

下面各平台工程主要用于“看某个芯片具体怎么配置 UART/SPI/CS”，不建议客户从多个平台目录里混拷不同版本驱动。

UART FIFO 溢出风险：G356 UART 是连续输出流。真实控制项目中不要依赖低频主循环直接轮询 MCU 硬件 UART FIFO；如果主循环里有显示、printf、PID、无线通信等耗时任务，可能导致 overrun、丢帧或姿态跳变。推荐使用 UART RX 中断或 DMA，打印调试信息也要低频输出。

## 目录选择

| 用户手里的主控/工具链 | UART 示例 | SPI 示例 | 说明 |
| :--- | :--- | :--- | :--- |
| TI LP-MSPM0G3507 / CCS | `MSPM0G3507/UART/` | `MSPM0G3507/SPI/` | 推荐优先试这个，和 G356 固件主控同系列 |
| TI MSPM0G3519 / CCS | `MSPM0G3519/UART/` | `MSPM0G3519/SPI/` | 64pin MSPM0G3519 参考工程 |
| STM32F103C8T6 / STM32Cube HAL | `STM32F103C8T6/UART/HAL/` | `STM32F103C8T6/SPI/HAL/` | CubeMX/HAL 风格 |
| STM32F103C8T6 / 标准库 | `STM32F103C8T6/UART/Standard/` | `STM32F103C8T6/SPI/Standard/` | Keil MDK 标准库风格 |
| STM32F407VET6 / STM32Cube HAL | `STM32F407VET6/UART/` | `STM32F407VET6/SPI/` | CubeMX/HAL 风格 |
| STM32F407VET6 / 标准库 | `STM32F407VET6/UART/Standard/` | `STM32F407VET6/SPI/Standard/` | Keil MDK 标准库风格 |

## 默认通信参数

UART 默认参数：

- 波特率：`115200`
- 数据位：`8`
- 校验位：无
- 停止位：`1`
- 流控：无

SPI 默认建议：

- 模式：Mode 0，`CPOL=0, CPHA=0`
- 位序：MSB first
- 数据位宽：8 bit
- 起步时钟：`2 MHz` 或更低（F407 示例因 APB2 分频限制使用 `1.3125MHz`）
- 每次片选拉低后连续读取当前模式的完整帧，读完再拉高片选；不要把同一帧拆成多次 CS 读取

## G356 上行帧

G356 默认输出 `48` 字节常用扩展遥测帧：

- 帧头：`AA 55`
- 类型：`04`
- 长度：`2E`
- 数据：加速度、角速度、Roll/Pitch/Yaw、温度、四元数
- 校验：字节 `2` 到 `45` 累加和取低 8 位
- 帧尾：`5A`

固定预设为：`02/36`兼容56字节、`04/2E`常用48字节、`03/46`完整72字节、`05/14`仅四元数22字节。自定义帧 Type 的最高位为1、低7位为数据组位图，Length 给出实际长度。推荐的 `G356_SDK` UART解析器可自动识别全部格式，SPI通过所选预设或位图设置读取长度。

切换命令建议发送两次、间隔约60ms：`0x26`选择0/1/2/3四种预设，`0x27`提交1..0x7F的数据组位图并进入自定义模式。恢复出厂回到48字节常用扩展格式；`0x19`应急恢复或RX拉低约3秒仍回到56字节兼容格式，便于旧主机恢复连接。

完整字段定义见仓库根目录 `communication_protocol.md` 的 G356 上行遥测章节。

## 老工程兼容说明

早期各平台目录里仍保留 `g356_driver.h` / `g356_driver.c`，便于已有客户工程继续使用：

- `g356_driver.h`
- `g356_driver.c`

新项目推荐改用 `G356_SDK/`，后续协议修正和易用性优化优先维护这一份。
