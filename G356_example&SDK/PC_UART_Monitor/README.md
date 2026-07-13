# PC UART Monitor

这个小工具用于在电脑上快速验证 G356 UART 输出。它会在串口字节流中搜索 `AA 55` 帧头，校验 56 字节遥测帧，并打印 Roll/Pitch/Yaw、加速度、角速度、温度和帧率。

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

