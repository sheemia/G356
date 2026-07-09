# G356 JYTech 6-Axis AHRS Gyroscope Module / JYTech G356 六轴 AHRS 陀螺仪模块

[中文](#中文说明) | [English](#english)

## 中文说明

## 技术支持与购买

如需获取产品资料、示例代码更新或使用支持：

### QQ 技术交流群：1047015163

如需购买 G356 模块，请在淘宝搜索 `JYTech`。
---


G356 是 JYTech 自主研发的六轴 AHRS 陀螺仪模块，内部集成三轴陀螺仪、三轴加速度计和姿态解算固件，可通过 UART 或 SPI 输出加速度、角速度、姿态角、温度以及原始惯性数据。本仓库提供 G356 的用户示例代码、上位机安装包和通讯协议文档，方便用户快速评估模块并将驱动代码移植到自己的产品中。

### 仓库内容

| 路径 | 内容 |
| --- | --- |
| `G356示例代码/` | MCU 端接收示例工程，演示如何通过 UART/SPI 读取并解析 G356 遥测帧。 |
| `Tools/JYTechAhrsStudio_v1.2.0_Setup.exe` | JYTech AHRS Studio 上位机安装包。 |
| `Tools/JYTech_AHRS_Studio_Communication Protocol_V1.0.pdf` | 上位机通讯协议文档。 |
| `技术交流群1047015163.txt` | 技术交流群信息。 |
| `产品购买：淘宝搜索【JYTech】.txt` | 购买入口提示。 |

### 已提供的示例工程

| MCU/平台 | 通讯方式 | 工程类型 |
| --- | --- | --- |
| MSPM0G3507 | SPI / UART | TI CCS / DriverLib |
| MSPM0G3519 | SPI / UART | TI CCS / DriverLib |
| STM32F103C8T6 | SPI / UART | STM32 HAL、STM32 Standard Peripheral Library |
| STM32F407VET6 | SPI / UART | STM32Cube HAL、STM32 Standard Peripheral Library / Keil MDK |

每个示例工程目录下都带有独立 `README.md`，包含接线方式、工程打开方式、数据帧结构和代码移植说明。建议优先阅读目标平台目录内的 README。

### G356 遥测帧概要

G356 默认输出固定 56 字节遥测帧：

- 帧头：`0xAA 0x55`
- 类型：`0x02`
- 长度：`0x36`
- 加速度：`int16_t`，除以 `2048` 转换为 g
- 角速度：`int16_t`，除以 `8.2` 转换为 dps，对应陀螺仪量程 ±4000 dps
- 姿态角：Roll / Pitch / Yaw，`float`，单位 degree
- 温度：`int16_t`，除以 `100` 转换为摄氏度
- 原始浮点六轴数据：Raw Accel / Raw Gyro，用于标定和温度补偿分析
- 校验：字节 2 到字节 53 累加和低 8 位
- 帧尾：`0x5A`

完整字段定义请参考各示例工程 README 或 `Tools` 目录下的通讯协议文档。

### 快速开始

1. 根据自己的 MCU 平台进入 `G356示例代码/` 下对应目录。
2. 阅读该目录内的 `README.md`，按接线表连接 G356 模块。
3. 使用对应 IDE 打开工程，例如 TI CCS、Keil MDK 或 STM32CubeIDE/Keil 工程。
4. 编译、下载到开发板。
5. 打开串口调试工具或 JYTech AHRS Studio，查看实时姿态和惯性数据。

### 上位机

JYTech AHRS Studio 可用于实时显示 G356 姿态数据、查看原始数据、配置输出速率和设备参数。Windows 用户可直接运行 `Tools/JYTechAhrsStudio_v1.2.0_Setup.exe` 安装。

### 注意事项

- G356 支持 UART 和 SPI 输出；下行配置命令请以通讯协议文档说明为准。
- SPI 读取时，主机需要在一次 CS 低电平周期内连续读完整 56 字节帧。
- 所有多字节整数和浮点数均为小端序。
- 不同示例工程的引脚定义不同，请以对应目录内 README 和源码为准。
- 本仓库包含第三方 MCU 厂商库文件，相关版权和许可请以原厂文件为准。

---

## English

## Support and Purchase

For product documentation, example-code updates, or technical support:

### Technical Support Group: 1047015163

To purchase the G356 module, search `JYTech` on Taobao.
---


G356 is a 6-axis AHRS gyroscope module developed by JYTech. It integrates a 3-axis gyroscope, a 3-axis accelerometer, and onboard attitude-fusion firmware. The module can stream acceleration, angular-rate, attitude angle, temperature, and raw inertial data through UART or SPI. This repository provides receiver examples, the JYTech AHRS Studio host tool, and protocol documentation to help users evaluate the module and port the driver code into their own products.

### Repository Contents

| Path | Description |
| --- | --- |
| `G356示例代码/` | MCU receiver examples showing how to read and parse G356 telemetry over UART/SPI. |
| `Tools/JYTechAhrsStudio_v1.2.0_Setup.exe` | JYTech AHRS Studio installer for Windows. |
| `Tools/JYTech_AHRS_Studio_Communication Protocol_V1.0.pdf` | Host communication protocol document. |
| `技术交流群1047015163.txt` | Technical support group information. |
| `产品购买：淘宝搜索【JYTech】.txt` | Purchase information. |

### Example Projects

| MCU / Platform | Interface | Project Type |
| --- | --- | --- |
| MSPM0G3507 | SPI / UART | TI CCS / DriverLib |
| MSPM0G3519 | SPI / UART | TI CCS / DriverLib |
| STM32F103C8T6 | SPI / UART | STM32 HAL and STM32 Standard Peripheral Library |
| STM32F407VET6 | SPI / UART | STM32Cube HAL and STM32 Standard Peripheral Library / Keil MDK |

Each example directory contains its own `README.md` with wiring, project setup, packet layout, and porting notes. Please read the README in the target platform directory first.

### G356 Telemetry Frame Summary

G356 outputs a fixed 56-byte telemetry frame by default:

- Header: `0xAA 0x55`
- Type: `0x02`
- Length: `0x36`
- Acceleration: `int16_t`, divided by `2048` to get g
- Angular rate: `int16_t`, divided by `8.2` to get dps, matching the ±4000 dps gyro full-scale range
- Attitude: Roll / Pitch / Yaw, `float`, in degrees
- Temperature: `int16_t`, divided by `100` to get degrees Celsius
- Raw floating-point 6-axis data: Raw Accel / Raw Gyro, for calibration and temperature-compensation analysis
- Checksum: low 8 bits of the sum from byte 2 to byte 53
- Tail: `0x5A`

For the complete packet definition, see the README files in the example directories or the protocol document in `Tools`.

### Quick Start

1. Open the matching platform directory under `G356示例代码/`.
2. Read the local `README.md` and wire the G356 module according to the pin table.
3. Open the project with the corresponding IDE, such as TI CCS, Keil MDK, STM32CubeIDE, or another supported workflow.
4. Build and flash the firmware to the development board.
5. Use a serial terminal or JYTech AHRS Studio to view real-time attitude and inertial data.

### Host Software

JYTech AHRS Studio can display G356 attitude data in real time, monitor raw data, and configure output rate and device parameters. Windows users can install it with `Tools/JYTechAhrsStudio_v1.2.0_Setup.exe`.

### Notes

- G356 supports UART and SPI telemetry output. Downlink configuration commands should follow the protocol document.
- For SPI reads, the host must read the full 56-byte frame during one continuous CS-low transaction.
- All multi-byte integers and floating-point fields are little-endian.
- Pin assignments differ between examples. Always check the README and source code in the selected example directory.
- This repository includes third-party MCU vendor library files. Their original copyright and license terms apply.

