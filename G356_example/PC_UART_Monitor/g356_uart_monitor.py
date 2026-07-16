#!/usr/bin/env python3
"""G356 UART telemetry monitor.

Usage:
    python g356_uart_monitor.py COM5
    python g356_uart_monitor.py COM5 --baud 115200 --print-every 10
    python g356_uart_monitor.py --list
"""

from __future__ import annotations

import argparse
import struct
import sys
import time
from dataclasses import dataclass

FRAME_SIZE = 56
HEADER = b"\xAA\x55"
TAIL = 0x5A
TYPE_TELEMETRY = 0x02
LENGTH_TELEMETRY = 0x36

ACCEL_LSB_PER_G = 2048.0
GYRO_LSB_PER_DPS = 8.2
TEMP_LSB_PER_DEGC = 100.0


@dataclass
class G356Data:
    accel_x: float
    accel_y: float
    accel_z: float
    gyro_x: float
    gyro_y: float
    gyro_z: float
    roll: float
    pitch: float
    yaw: float
    temp: float
    raw_accel_x: float
    raw_accel_y: float
    raw_accel_z: float
    raw_gyro_x: float
    raw_gyro_y: float
    raw_gyro_z: float


class G356FrameParser:
    def __init__(self) -> None:
        self._state = 0
        self._buf = bytearray()

    def feed(self, byte: int) -> bytes | None:
        if self._state == 0:
            if byte == HEADER[0]:
                self._buf = bytearray([byte])
                self._state = 1
            return None

        if self._state == 1:
            if byte == HEADER[1]:
                self._buf.append(byte)
                self._state = 2
            elif byte == HEADER[0]:
                self._buf = bytearray([byte])
            else:
                self._buf.clear()
                self._state = 0
            return None

        self._buf.append(byte)
        if len(self._buf) < FRAME_SIZE:
            return None

        frame = bytes(self._buf)
        self._buf.clear()
        self._state = 0
        return frame if validate_frame(frame) else b""


def validate_frame(frame: bytes) -> bool:
    if len(frame) != FRAME_SIZE:
        return False
    if frame[0:2] != HEADER:
        return False
    if frame[2] != TYPE_TELEMETRY or frame[3] != LENGTH_TELEMETRY:
        return False
    if frame[-1] != TAIL:
        return False
    checksum = sum(frame[2:54]) & 0xFF
    return checksum == frame[54]


def parse_frame(frame: bytes) -> G356Data:
    ax, ay, az, gx, gy, gz = struct.unpack_from("<hhhhhh", frame, 4)
    roll, pitch, yaw = struct.unpack_from("<fff", frame, 16)
    (temp_raw,) = struct.unpack_from("<h", frame, 28)
    raw_values = struct.unpack_from("<ffffff", frame, 30)

    return G356Data(
        accel_x=ax / ACCEL_LSB_PER_G,
        accel_y=ay / ACCEL_LSB_PER_G,
        accel_z=az / ACCEL_LSB_PER_G,
        gyro_x=gx / GYRO_LSB_PER_DPS,
        gyro_y=gy / GYRO_LSB_PER_DPS,
        gyro_z=gz / GYRO_LSB_PER_DPS,
        roll=roll,
        pitch=pitch,
        yaw=yaw,
        temp=temp_raw / TEMP_LSB_PER_DEGC,
        raw_accel_x=raw_values[0],
        raw_accel_y=raw_values[1],
        raw_accel_z=raw_values[2],
        raw_gyro_x=raw_values[3],
        raw_gyro_y=raw_values[4],
        raw_gyro_z=raw_values[5],
    )


def import_serial():
    try:
        import serial
        import serial.tools.list_ports
    except ImportError:
        print("缺少 pyserial，请先执行：python -m pip install pyserial", file=sys.stderr)
        raise SystemExit(2)
    return serial


def list_ports() -> None:
    serial = import_serial()
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("未发现串口。请检查 USB 转串口是否已插入、驱动是否正常。")
        return
    for port in ports:
        print(f"{port.device}\t{port.description}")


def monitor(port: str, baud: int, print_every: int) -> None:
    serial = import_serial()
    parser = G356FrameParser()
    valid_count = 0
    invalid_count = 0
    start_time = time.monotonic()

    with serial.Serial(port, baudrate=baud, bytesize=8, parity="N", stopbits=1, timeout=1) as ser:
        print(f"Listening on {port} @ {baud} 8N1. Press Ctrl+C to stop.")
        while True:
            chunk = ser.read(256)
            if not chunk:
                print("等待数据中... 请检查供电、G356 TXD->接收端 RXD、GND 共地、波特率。")
                continue

            for byte in chunk:
                frame = parser.feed(byte)
                if frame is None:
                    continue
                if frame == b"":
                    invalid_count += 1
                    continue

                valid_count += 1
                if valid_count % print_every != 0:
                    continue

                data = parse_frame(frame)
                elapsed = max(time.monotonic() - start_time, 0.001)
                fps = valid_count / elapsed
                print(
                    f"roll={data.roll:8.2f} pitch={data.pitch:8.2f} yaw={data.yaw:8.2f} "
                    f"acc=[{data.accel_x:7.3f},{data.accel_y:7.3f},{data.accel_z:7.3f}]g "
                    f"gyro=[{data.gyro_x:8.2f},{data.gyro_y:8.2f},{data.gyro_z:8.2f}]dps "
                    f"temp={data.temp:5.1f}C valid={valid_count} invalid={invalid_count} fps={fps:5.1f}"
                )


def main() -> int:
    argp = argparse.ArgumentParser(description="G356 UART telemetry monitor")
    argp.add_argument("port", nargs="?", help="串口号，例如 COM5 或 /dev/ttyUSB0")
    argp.add_argument("--baud", type=int, default=115200, help="串口波特率，默认 115200")
    argp.add_argument("--print-every", type=int, default=10, help="每收到多少帧打印一次，默认 10")
    argp.add_argument("--list", action="store_true", help="列出当前可用串口")
    args = argp.parse_args()

    if args.list:
        list_ports()
        return 0

    if not args.port:
        argp.print_help()
        return 2

    try:
        monitor(args.port, args.baud, max(args.print_every, 1))
    except KeyboardInterrupt:
        print("\n已停止。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
