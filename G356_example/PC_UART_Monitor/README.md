# PC UART Monitor

这个小工具用于在电脑上快速验证 G356 UART 输出。它会在串口字节流中搜索 `AA 55` 帧头，自动校验56/48/72/22字节预设及自定义数据组动态帧，并打印当前帧实际包含的数据和帧率；未发送的字段显示 `--`。

工具会自动兼容56字节旧帧的历史 Roll/Pitch 槽位，终端打印的名称始终对应模块物理 Roll/Pitch。

## 使用方法

```powershell
python -m pip install pyserial
python g356_uart_monitor.py COM5
```

可选参数：

```powershell
python g356_uart_monitor.py COM5 --baud 115200 --print-every 10
```

如果不知道串口号：

```powershell
python g356_uart_monitor.py --list
```

## 适用场景

- 客户刚拿到模块，先确认供电、线序、波特率都正确。
- MCU 工程暂时没跑通时，用 PC 把模块本身排除掉。
- 采集少量文本数据给技术支持定位问题。

