# G356 AHRS 陀螺仪模块 UART 通信接收示例工程 (STM32F103C8T6, StdPeriph 标准库)

## 技术支持与购买

如需获取产品资料、示例代码更新或使用支持：

### QQ 技术交流群：1047015163

如需购买 G356 模块，请在淘宝搜索 `JYTech`。
---

G356 是 JYTech 自主研发的六轴 AHRS 陀螺仪模块，内部集成三轴陀螺仪、三轴加速度计和姿态解算固件，可输出加速度、角速度、姿态角、温度以及原始惯性数据。本示例工程用于演示如何在不同 MCU 平台上通过 SPI 或 UART 接收并解析 G356 遥测数据，便于用户快速评估模块并将驱动代码移植到自己的产品中。

本工程是针对 **G356 六轴姿态传感器模块** 的 UART 通信示例代码，运行在 **STM32F103C8T6 最小系统板** 上，使用 **STM32F10x_StdPeriph_Lib V3.5.0 标准库**（不是 HAL 库）。本工程通过 USART2 接口持续接收 G356 模块主动发送的姿态及惯性传感器遥测数据，并通过另一路串口 (USART1) 将解析后的数据以 115200 波特率打印输出，便于用户查看和二次开发。

可直接复用到你自己项目里的驱动代码是 `HARDWARE/G356/g356_driver.h` / `g356_driver.c`（见第 4 节），不需要连带这个 demo 的主循环一起拷贝。

本工程原本是一个完全空的 Keil 工程（连库文件都没拷进去），现已 vendor 了官方 `STM32F10x_StdPeriph_Lib_V3.5.0`（`Libraries/STM32F10x_StdPeriph_Driver` + `Libraries/CMSIS`）、写好了 `USER`/`SYSTEM`/`HARDWARE` 各层代码，并在 `Standard.uvprojx` 里手工组装好了文件列表和宏定义/Include 路径。已用 Keil `UV4.exe -b` 离线编译验证通过（0 Error, 0 Warning）。

---

## 1. 硬件连接 (Hardware Connections)

请按以下引脚映射关系连接 G356 模块与你的 STM32F103C8T6 最小系统板：

| G356 模块引脚 | STM32F103C8T6 引脚 | 功能说明 | 备注 |
| :--- | :--- | :--- | :--- |
| **VIN** | **5V（推荐）** | 模块电源输入，3.3V~12V | 带 -20V 防反接保护，请确保供电稳定 |
| **GND** | **GND** | 电源地 | 必须共地 |
| **TXD (G356发送)** | **PA3 (USART2_RX)** | G356 持续主动发送遥测数据帧 | |
| **RXD (G356接收)** | **PA2 (USART2_TX)** | 仅在你需要向 G356 发送下行控制指令时才需要接 | |

> 调试输出用另一路 USART1 (TX=PA9 / RX=PA10)，外接 USB-TTL 模块，PC 端打开对应虚拟串口（115200, 8N1）即可看到解析后的数据，见第 5 节。
>
> 注意：UART 接口不像 SPI 那样有片选信号天然分隔每一帧，G356 只要上电运行就会以约 100Hz 的速率持续往外发送数据帧，接收端无需发送任何请求。
>
> 引脚选择与本仓库 `STM32F103C8T6/UART/HAL` 那个 HAL 版本完全一致（都用 USART1 调试 + USART2 链路），方便同一块板子在两个库版本之间切换测试。

---

## 2. 系统时钟与 UART 通信配置

* **系统时钟**：内部 HSI 8MHz 振荡器 / 2 -> PLL ×16 -> SYSCLK 64MHz。**`system_stm32f10x.c` 已改为 HSI 路径**（自定义 `SYSCLK_FREQ_64MHz_HSI` 宏 + 新增的 `SetSysClockToHSI64()`，替换掉 StdPeriph 默认的 `SYSCLK_FREQ_72MHz`/`SetSysClockTo72()` HSE 路径），由启动文件在进入 `main()` 之前自动调用 `SystemInit()` 完成，不需要在 `main.c` 里手写时钟配置代码。**故意不用外部 HSE 晶振**：不同厂家/批次的 F103C8T6 最小系统板实际焊的 HSE 晶振频率并不统一，原模板里 `HSE_VALUE` 是固定宏，一旦和板子实际晶振不一致，`RCC_GetClocksFreq()` 算出来的 USART 波特率分频值、`delay.c` 的 `fac_us` 都会跟着算错（且不会报错，只是悄悄变成乱码/计时不准）。HSI 是芯片内部出厂校准的振荡器，不存在这个隐患；代价是 F103 的 HSI/PLL 路径只能到 64MHz（HSI/2=4MHz 输入，×16 是最大倍频），比 8MHz HSE + ×9 PLL 能到的 72MHz 略低，对本示例而言可以接受。
* **波特率**：115200 bps（需与 G356 当前配置的波特率一致，出厂默认为 115200，可通过上位机/下行指令修改）。
* **数据位/校验位/停止位**：8 位数据，无校验，1 位停止位，即标准 **8N1** 配置。

---

## 3. 56 字节数据帧结构 (Telemetry Frame Structure)

G356 模块向外发送的数据为固定的 **56 字节** 遥测帧，结构与 SPI 接口完全一致：

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

* **`HARDWARE/G356/g356_driver.h` / `g356_driver.c`** —— 可直接整体拷贝复用的 G356 驱动模块，**不依赖任何 STM32/StdPeriph 接口**，纯字节流处理（用的是 `<stdint.h>`/`<stdbool.h>` 标准类型，不是 StdPeriph 的 `u8`/`u16`），可移植到任意 MCU，和 HAL 版本完全是同一份代码：
    1. **帧解析状态机** (`G356_FeedByte`)：UART 是连续字节流，没有片选信号天然分帧，所以驱动内部维护一个小状态机——逐字节喂给它，它自己搜索 `0xAA 0x55` 帧头（只在空闲态识别，避免被 payload 中偶然出现的同样字节误同步）、定长收集剩余 54 字节、校验类型/长度/校验和/帧尾。返回 `G356_FRAME_PENDING`（还没收完）/`G356_FRAME_VALID`（一帧校验通过）/`G356_FRAME_INVALID`（凑满 56 字节但校验失败，已自动复位重新搜索帧头）。
    2. **数据安全解析** (`G356_ParseData`)：用 `memcpy` 安全提取 `float` 数据；换算用乘以倒数代替浮点除法。
* **`SYSTEM/usart/usart.c`** —— `uart1_init()`/`uart1_send_byte()`（调试口）+ `uart2_init()`/`uart2_recv_byte()`（G356 链路，阻塞接收）。
* **`SYSTEM/delay/delay.c`** —— 基于 SysTick 的 `delay_us`/`delay_ms` 忙等待延时。
* **`SYSTEM/sys/sys.c`** —— `NVIC_Configuration()`，仅设置中断分组。
* **`USER/main.c`** —— demo 自身的演示代码：外设初始化、开机横幅、阻塞接收字节并喂给驱动、统计有效/无效帧次数、格式化打印到调试串口。**这部分（统计计数、打印间隔等）只是 demo 用来观察效果，集成到你自己的产品时可以整体删掉，只保留对 `G356_FeedByte`/`G356_ParseData` 的调用。**
* **`Libraries/`** —— vendor 进来的官方 `STM32F10x_StdPeriph_Lib_V3.5.0`，工程里只链接了实际用到的几个模块源文件（`misc.c`/`stm32f10x_gpio.c`/`stm32f10x_rcc.c`/`stm32f10x_usart.c`），头文件目录是完整的，方便以后加别的外设时直接在 `Standard.uvprojx` 里把对应 `.c` 加进工程即可。

---

## 5. 使用步骤

1. **导入工程**：打开 Keil MDK-ARM (uVision)，直接打开 `Standard.uvprojx`。
2. **编译与烧录**：点击编译按钮，确认编译通过（本工程已用 `UV4.exe -b` 离线验证过 0 Error/0 Warning）。使用 ST-Link/J-Link 等调试器将程序下载到 STM32F103C8T6 最小系统板。
3. **接线与运行**：按照第一节的引脚接线表将 G356 模块与开发板连接，USART1 接 USB-TTL 模块。
4. **打开串口调试工具**：在 PC 端打开任意串口助手（波特率 `115200`，数据位 `8`，停止位 `1`，无校验），即可看到实时输出的姿态数据与惯性原始数据：
    ```text
    =========================================
       G356 AHRS Module UART Receiver Demo   
       MCU: STM32F103C8T6 | Baudrate: 115200 
       Communication: USART2 (115200, 8N1)   
    =========================================
    Starting data reading...
    ANG[R:  0.12, P: -2.34, Y: 120.45] deg | ACC[ 0.004, -0.041,  1.002] g | GYR[   0.0,   0.0,   0.0] dps | Temp: 28.5 C
    ANG[R:  0.13, P: -2.33, Y: 120.45] deg | ACC[ 0.005, -0.042,  1.001] g | GYR[   0.0,   0.0,   0.0] dps | Temp: 28.5 C
    ```

