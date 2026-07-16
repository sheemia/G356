# G356 AHRS 陀螺仪模块 SPI 接收示例工程 (STM32F407VET6, StdPeriph)

## 技术支持与购买

如需获取产品资料、示例代码更新或使用支持：

### QQ 技术交流群：1047015163

如需购买 G356 模块，请在淘宝搜索 `JYTech`。
---

G356 是 JYTech 自主研发的六轴 AHRS 陀螺仪模块，内部集成三轴陀螺仪、三轴加速度计和姿态解算固件，可输出加速度、角速度、姿态角、温度以及原始惯性数据。本示例工程用于演示如何在不同 MCU 平台上通过 SPI 或 UART 接收并解析 G356 遥测数据，便于用户快速评估模块并将驱动代码移植到自己的产品中。

本工程使用 STM32F4xx Standard Peripheral Library，在 STM32F407VET6 上通过 SPI1 读取 G356 模块的 56 字节遥测帧，并通过 USART1 打印解析结果。

## 引脚连接

| G356 模块 | STM32F407VET6 | 说明 |
| --- | --- | --- |
| VIN | 5V（推荐） | 模块供电，3.3V~12V，带 -20V 防反接保护 |
| GND | GND | 共地 |
| CS | PA4 | SPI1 片选，低有效 |
| SCLK | PA5 | SPI1_SCK |
| MISO | PA6 | SPI1_MISO |
| MOSI | PA7 | SPI1_MOSI，可接 dummy 输出 |
| 调试串口 TX | PA9 | USART1_TX，接 USB-TTL RX |
| 调试串口 RX | PA10 | USART1_RX，通常可不接 |

SPI 模式为 Mode 0，示例配置约 2.625MHz。G356 每次 CS 拉低后主机连续读取 56 字节。

## 56 字节遥测帧

| 偏移 | 字段 | 类型 | 说明 |
| --- | --- | --- | --- |
| 0 | Header0 | uint8_t | 0xAA |
| 1 | Header1 | uint8_t | 0x55 |
| 2 | Type | uint8_t | 0x02 |
| 3 | Length | uint8_t | 0x36 |
| 4 ~ 9 | Accel X/Y/Z | int16_t | 除以 2048 转换为 g |
| 10 ~ 15 | Gyro X/Y/Z | int16_t | 除以 8.2 转换为 dps |
| 16 ~ 27 | Roll/Pitch/Yaw | float | 单位 degree |
| 28 ~ 29 | Temperature | int16_t | 除以 100 转换为摄氏度 |
| 30 ~ 53 | Raw Accel/Gyro | float[6] | 未量化、未扣校准 offset 的原始浮点值 |
| 54 | Checksum | uint8_t | 字节 2 到 53 的累加和 |
| 55 | Tail | uint8_t | 0x5A |

## 代码结构

- `HARDWARE/G356/g356_driver.c/h`：协议校验和字段换算，陀螺仪量程为 ±4000dps，`G356_GYRO_LSB_PER_DPS = 8.2f`。
- `HARDWARE/SPI/spi.c/h`：SPI1 + PA4 片选的 56 字节 burst read。
- `SYSTEM/usart`：USART1 调试输出。
- `USER/main.c`：168MHz HSI PLL 时钟、外设初始化、周期读取和打印。

Keil 中打开 `Standard.uvprojx` 即可编译。

