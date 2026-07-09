# G356 AHRS 陀螺仪模块 SPI 通信接收示例工程 (STM32F103C8T6, StdPeriph 标准库)

## 技术支持与购买

如需获取产品资料、示例代码更新或使用支持：

### QQ 技术交流群：1047015163

如需购买 G356 模块，请在淘宝搜索 `JYTech`。
---

G356 是 JYTech 自主研发的六轴 AHRS 陀螺仪模块，内部集成三轴陀螺仪、三轴加速度计和姿态解算固件，可输出加速度、角速度、姿态角、温度以及原始惯性数据。本示例工程用于演示如何在不同 MCU 平台上通过 SPI 或 UART 接收并解析 G356 遥测数据，便于用户快速评估模块并将驱动代码移植到自己的产品中。

本工程是针对 **G356 六轴姿态传感器模块** 的 SPI 通信示例代码，运行在 **STM32F103C8T6 最小系统板** 上，使用 **STM32F10x_StdPeriph_Lib V3.5.0 标准库**（不是 HAL 库）。本工程作为 **SPI 主机 (Controller)** 运行，通过 SPI2 接口读取作为 **SPI 从机 (Peripheral)** 的 G356 模块的姿态及惯性传感器数据，并通过另一路串口 (USART1) 将解析后的数据以 115200 波特率打印输出，便于用户查看和二次开发。

可直接复用到你自己项目里的驱动代码是 `HARDWARE/G356/g356_driver.h` / `g356_driver.c`（依赖同目录下的 `HARDWARE/SPI/spi.h`/`spi.c`，见第 4 节），不需要连带这个 demo 的主循环一起拷贝。

本工程原本是一个完全空的 Keil 工程（连库文件都没拷进去），现已 vendor 了官方 `STM32F10x_StdPeriph_Lib_V3.5.0`（`Libraries/STM32F10x_StdPeriph_Driver` + `Libraries/CMSIS`）、写好了 `USER`/`SYSTEM`/`HARDWARE` 各层代码，并在 `Standard.uvprojx` 里手工组装好了文件列表和宏定义/Include 路径。已用 Keil `UV4.exe -b` 离线编译验证通过（0 Error, 0 Warning）。

---

## 1. 硬件连接 (Hardware Connections)

请按以下引脚映射关系连接 G356 模块与你的 STM32F103C8T6 最小系统板：

| G356 模块引脚 | STM32F103C8T6 引脚 | 功能说明 | 备注 |
| :--- | :--- | :--- | :--- |
| **VCC** | **3.3V** | 模块电源输入 (3.3V) | 请确保供电稳定 |
| **GND** | **GND** | 电源地 | 必须共地 |
| **CS (片选)** | **PA2** | SPI 片选信号 (GPIO 软件输出) | 主机拉低启动通信，拉高结束；NSS 软件管理，不占用硬件 NSS 引脚 |
| **SCLK (时钟)** | **PB13 (SPI2_SCK)** | SPI 时钟信号 | 时钟频率 ~2MHz (APB1=32MHz/16) |
| **MOSI (主机输出)** | **PB15 (SPI2_MOSI)** | SPI 主机输出从机输入 | 物理接线可悬空不接，G356 仅发送数据 |
| **MISO (主机输入)** | **PB14 (SPI2_MISO)** | SPI 主机输入从机输出 | 从机返回数据帧 |

> 调试输出用板上 USART1 (TX=PA9 / RX=PA10)，外接 USB-TTL 模块，PC 端打开对应虚拟串口（115200, 8N1）即可看到解析后的数据，见第 5 节。
>
> 引脚选择与本仓库 `STM32F103C8T6/SPI/HAL` 那个 HAL 版本完全一致（都用 SPI2/PB13-15/CS-PA2），方便同一块板子在两个库版本之间切换测试。

---

## 2. 系统时钟与 SPI 通信配置

* **系统时钟**：内部 HSI 8MHz 振荡器 / 2 -> PLL ×16 -> SYSCLK 64MHz。**`system_stm32f10x.c` 已改为 HSI 路径**（自定义 `SYSCLK_FREQ_64MHz_HSI` 宏 + 新增的 `SetSysClockToHSI64()`，替换掉 StdPeriph 默认的 `SYSCLK_FREQ_72MHz`/`SetSysClockTo72()` HSE 路径），由启动文件在进入 `main()` 之前自动调用 `SystemInit()` 完成，不需要在 `main.c` 里手写时钟配置代码。**故意不用外部 HSE 晶振**：不同厂家/批次的 F103C8T6 最小系统板实际焊的 HSE 晶振频率并不统一，原模板里 `HSE_VALUE` 是固定宏，一旦和板子实际晶振不一致，`RCC_GetClocksFreq()` 算出来的 USART 波特率分频值、`delay.c` 的 `fac_us` 都会跟着算错（且不会报错，只是悄悄变成乱码/计时不准）。HSI 是芯片内部出厂校准的振荡器，不存在这个隐患；代价是 F103 的 HSI/PLL 路径只能到 64MHz（HSI/2=4MHz 输入，×16 是最大倍频），比 8MHz HSE + ×9 PLL 能到的 72MHz 略低，对本示例而言可以接受。
* **SPI 模式**：Mode 0 (CPOL=0, CPHA=0)，`SPI_Mode_Master`，8 位字长，MSB 先传，NSS 软件管理（CS 由 GPIO PA2 手动控制，见 `HARDWARE/SPI/spi.c`）。
* **通信时钟**：APB1(32MHz) / 16 = **2MHz**，与 MSPM0G3507 参考实现的 2MHz 一致；G356 是从机，时钟完全由主机决定，理论上可以调快，但本示例没有在真实硬件上验证过更高速率。

---

## 3. 56 字节数据帧结构 (Telemetry Frame Structure)

G356 模块向主机发送的数据为固定的 **56 字节** 遥测帧，结构与 MSPM0G3507 参考实现完全一致：

| 字节偏移 | 字段名称 | 数据类型 | 说明 |
| :--- | :--- | :--- | :--- |
| **0 ~ 1** | 帧头 (Header) | `uint8_t[2]` | 固定为 `0xAA 0x55` |
| **2** | 数据类型 (Type) | `uint8_t` | 固定为 `0x02` (数据包标识) |
| **3** | 数据长度 (Length) | `uint8_t` | 固定为 `0x36` (54字节，表示后续数据+校验和长度) |
| **4 ~ 5** | Accel X | `int16_t` | 加速度计 X 轴原始数据。除以 2048.0 转换为 $g$ |
| **6 ~ 7** | Accel Y | `int16_t` | 加速度计 Y 轴原始数据。除以 2048.0 转换为 $g$ |
| **8 ~ 9** | Accel Z | `int16_t` | 加速度计 Z 轴原始数据。除以 2048.0 转换为 $g$ |
| **10 ~ 11** | Gyro X | `int16_t` | 陀螺仪 X 轴原始数据。除以 8.2 转换为度/秒 (dps) |
| **12 ~ 13** | Gyro Y | `int16_t` | 陀螺仪 Y 轴原始数据。除以 8.2 转换为度/秒 (dps) |
| **14 ~ 15** | Gyro Z | `int16_t` | 陀螺仪 Z 轴原始数据。除以 8.2 转换为度/秒 (dps) |
| **16 ~ 19** | Roll | `float` | 横滚角 Roll，单位为度 (°) |
| **20 ~ 23** | Pitch | `float` | 俯仰角 Pitch，单位为度 (°) |
| **24 ~ 27** | Yaw | `float` | 偏航角 Yaw，单位为度 (°) |
| **28 ~ 29** | Temperature | `int16_t` | 模块内部温度原始数据。除以 100.0 转换为 ℃ |
| **30 ~ 33** | Raw Accel X | `float` | 未量化、未扣校准offset的加速度计原始浮点值，单位 $g$ |
| **34 ~ 37** | Raw Accel Y | `float` | 同上 Y 轴 |
| **38 ~ 41** | Raw Accel Z | `float` | 同上 Z 轴 |
| **42 ~ 45** | Raw Gyro X | `float` | 未量化、未扣校准offset的陀螺仪原始浮点值，单位 dps |
| **46 ~ 49** | Raw Gyro Y | `float` | 同上 Y 轴 |
| **50 ~ 53** | Raw Gyro Z | `float` | 同上 Z 轴 |
| **54** | 和校验 (Checksum)| `uint8_t` | 字节 2 至 53 的累加和的低 8 位 |
| **55** | 帧尾 (Tail) | `uint8_t` | 固定为 `0x5A` |

---

## 4. 代码结构

* **`HARDWARE/G356/g356_driver.h` / `g356_driver.c`** —— 可直接整体拷贝复用的 G356 驱动模块：
    1. **全双工逐字节突发读取** (`G356_ReadPacket`)：拉低片选 (PA2)，用 `SPI2_ReadWriteByte()` 连续读取 56 字节（CS 全程保持低电平，避免帧内字节被中途撕裂），读取完成后拉高片选，过滤不合法的帧头、帧尾、和校验。
    2. **数据安全解析** (`G356_ParseData`)：用 `memcpy` 安全提取 `float` 数据，避免 Cortex-M3 非对齐内存访问问题；换算用乘以倒数代替浮点除法。
    * 依赖 `HARDWARE/SPI/spi.h` 的 `SPI2_ReadWriteByte()` 和 `G356_CS_*` 宏，复用到自己项目时把这两个文件一起拷过去，按需调整里面的引脚/外设名字。
* **`HARDWARE/SPI/spi.c`** —— `SPI2_Init()`，StdPeriph 风格的 SPI2 外设 + CS 引脚初始化。
* **`SYSTEM/usart/usart.c`** —— `uart1_init()` / `uart1_send_byte()`，调试串口初始化与发送（没有走 `printf`/`fputc` 重定向，和仓库里其它示例一样用自实现的轻量打印函数，避免引入标准库浮点格式化支持）。
* **`SYSTEM/delay/delay.c`** —— 基于 SysTick 的 `delay_us`/`delay_ms` 忙等待延时（ALIENTEK 经典实现，去掉了未使用的 ucos 分支）。
* **`SYSTEM/sys/sys.c`** —— `NVIC_Configuration()`，仅设置中断分组。
* **`USER/main.c`** —— demo 自身的演示代码：外设初始化、开机横幅、10ms 轮询调用驱动读取+解析、统计成功/失败次数、格式化打印到调试串口。**这部分（重试次数、统计计数、打印间隔等）只是 demo 用来观察效果，集成到你自己的产品时可以整体删掉，只保留对 `G356_ReadPacket`/`G356_ParseData` 的调用。**
* **`Libraries/`** —— vendor 进来的官方 `STM32F10x_StdPeriph_Lib_V3.5.0`（`STM32F10x_StdPeriph_Driver` + `CMSIS`），工程里只链接了实际用到的几个模块源文件（`misc.c`/`stm32f10x_gpio.c`/`stm32f10x_rcc.c`/`stm32f10x_spi.c`/`stm32f10x_usart.c`），头文件目录是完整的，方便以后加别的外设时直接在 `Standard.uvprojx` 里把对应 `.c` 加进工程即可，不需要重新 vendor。

---

## 5. 使用步骤

1. **导入工程**：打开 Keil MDK-ARM (uVision)，直接打开 `Standard.uvprojx`。
2. **编译与烧录**：点击编译按钮，确认编译通过（本工程已用 `UV4.exe -b` 离线验证过 0 Error/0 Warning）。使用 ST-Link/J-Link 等调试器将程序下载到 STM32F103C8T6 最小系统板。
3. **接线与运行**：按照第一节的引脚接线表将 G356 模块与开发板连接，USART1 接 USB-TTL 模块。
4. **打开串口调试工具**：在 PC 端打开任意串口助手（波特率 `115200`，数据位 `8`，停止位 `1`，无校验），即可看到实时输出的姿态数据与惯性原始数据：
    ```text
    =========================================
       G356 AHRS Module SPI Receiver Demo    
       MCU: STM32F103C8T6 | Baudrate: 115200 
       Communication: SPI2 (Mode 0, ~2MHz)   
    =========================================
    Starting data reading...
    ANG[R:  0.12, P: -2.34, Y: 120.45] deg | ACC[ 0.004, -0.041,  1.002] g | GYR[   0.0,   0.0,   0.0] dps | Temp: 28.5 C
    ANG[R:  0.13, P: -2.33, Y: 120.45] deg | ACC[ 0.005, -0.042,  1.001] g | GYR[   0.0,   0.0,   0.0] dps | Temp: 28.5 C
    ```
