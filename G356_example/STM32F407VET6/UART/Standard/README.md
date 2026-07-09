# G356 AHRS 陀螺仪模块 UART 接收示例工程 (STM32F407VET6, StdPeriph)

## 技术支持与购买

如需获取产品资料、示例代码更新或使用支持：

### QQ 技术交流群：1047015163

如需购买 G356 模块，请在淘宝搜索 `JYTech`。
---

G356 是 JYTech 自主研发的六轴 AHRS 陀螺仪模块，内部集成三轴陀螺仪、三轴加速度计和姿态解算固件，可输出加速度、角速度、姿态角、温度以及原始惯性数据。本示例工程用于演示如何在不同 MCU 平台上通过 SPI 或 UART 接收并解析 G356 遥测数据，便于用户快速评估模块并将驱动代码移植到自己的产品中。

本工程使用 STM32F4xx Standard Peripheral Library，在 STM32F407VET6 上通过 USART2 接收 G356 模块主动输出的 56 字节遥测帧，并通过 USART1 打印解析结果。

## 引脚连接

| G356 模块 | STM32F407VET6 | 说明 |
| --- | --- | --- |
| VCC | 3.3V | 模块供电 |
| GND | GND | 共地 |
| TXD | PA3 / USART2_RX | G356 输出遥测数据 |
| RXD | PA2 / USART2_TX | 需要发送下行命令时连接 |
| 调试串口 TX | PA9 / USART1_TX | 接 USB-TTL RX |
| 调试串口 RX | PA10 / USART1_RX | 通常可不接 |

G356 UART 出厂默认 115200, 8N1；如果你已通过上位机修改波特率，请同步调整 `USART2_G356_Init()` 的参数。

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

- `HARDWARE/G356/g356_driver.c/h`：UART 字节流状态机、协议校验和字段换算，陀螺仪量程为 ±4000dps，`G356_GYRO_LSB_PER_DPS = 8.2f`。
- `SYSTEM/usart`：USART1 调试输出、USART2 G356 链路。
- `USER/main.c`：168MHz HSI PLL 时钟、外设初始化、逐字节接收和打印。

Keil 中打开 `Standard.uvprojx` 即可编译。
