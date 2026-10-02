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

LEGACY_FRAME_SIZE = 56
COMMON_FRAME_SIZE = 48
QUATERNION_FRAME_SIZE = 72
QUAT_ONLY_FRAME_SIZE = 22
HEADER = b"\xAA\x55"
TAIL = 0x5A
TYPE_TELEMETRY = 0x02
LENGTH_TELEMETRY = 0x36
TYPE_QUATERNION = 0x03
LENGTH_QUATERNION = 0x46
TYPE_COMMON = 0x04
LENGTH_COMMON = 0x2E
TYPE_QUAT_ONLY = 0x05
LENGTH_QUAT_ONLY = 0x14

FIELD_ACCEL = 0x01
FIELD_GYRO = 0x02
FIELD_EULER = 0x04
FIELD_TEMP = 0x08
FIELD_RAW_ACCEL = 0x10
FIELD_RAW_GYRO = 0x20
FIELD_QUATERNION = 0x40
FIELD_SIZES = (6, 6, 12, 2, 12, 12, 16)

ACCEL_LSB_PER_G = 2048.0
GYRO_LSB_PER_DPS = 8.2
TEMP_LSB_PER_DEGC = 100.0


@dataclass
class G356Data:
    valid_fields: int
    accel: tuple[float, float, float] | None
    gyro: tuple[float, float, float] | None
    euler: tuple[float, float, float] | None
    temp: float | None
    raw_accel: tuple[float, float, float] | None
    raw_gyro: tuple[float, float, float] | None
    quaternion: tuple[float, float, float, float] | None


def field_mask_and_size(data_type: int, length: int) -> tuple[int, int]:
    fixed_masks = {
        (TYPE_TELEMETRY, LENGTH_TELEMETRY): 0x3F,
        (TYPE_QUATERNION, LENGTH_QUATERNION): 0x7F,
        (TYPE_COMMON, LENGTH_COMMON): 0x4F,
        (TYPE_QUAT_ONLY, LENGTH_QUAT_ONLY): 0x40,
    }
    mask = fixed_masks.get((data_type, length), data_type & 0x7F if data_type & 0x80 else 0)
    size = 6 + sum(size for bit, size in enumerate(FIELD_SIZES) if mask & (1 << bit))
    return (mask, size) if mask and length == size - 2 else (0, 0)


class G356FrameParser:
    def __init__(self) -> None:
        self._state = 0
        self._buf = bytearray()
        self._expected_size = LEGACY_FRAME_SIZE

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
        if len(self._buf) == 4:
            _, self._expected_size = field_mask_and_size(self._buf[2], self._buf[3])
            if not self._expected_size:
                self._buf.clear()
                self._state = 0
                return b""
        if len(self._buf) < self._expected_size:
            return None

        frame = bytes(self._buf)
        self._buf.clear()
        self._state = 0
        return frame if validate_frame(frame) else b""


def validate_frame(frame: bytes) -> bool:
    if len(frame) < 6:
        return False
    _, expected_size = field_mask_and_size(frame[2], frame[3])
    if not expected_size or len(frame) != expected_size:
        return False
    if frame[0:2] != HEADER:
        return False
    if frame[-1] != TAIL:
        return False
    checksum = sum(frame[2:-2]) & 0xFF
    return checksum == frame[-2]


def parse_frame(frame: bytes) -> G356Data:
    mask, _ = field_mask_and_size(frame[2], frame[3])
    offset = 4
    accel = gyro = euler = raw_accel = raw_gyro = quaternion = None
    temp = None
    if mask & FIELD_ACCEL:
        values = struct.unpack_from("<hhh", frame, offset); offset += 6
        accel = tuple(value / ACCEL_LSB_PER_G for value in values)
    if mask & FIELD_GYRO:
        values = struct.unpack_from("<hhh", frame, offset); offset += 6
        gyro = tuple(value / GYRO_LSB_PER_DPS for value in values)
    if mask & FIELD_EULER:
        first_angle, second_angle, yaw = struct.unpack_from("<fff", frame, offset); offset += 12
        if frame[2] == TYPE_TELEMETRY:
            euler = (second_angle, first_angle, yaw)
        else:
            euler = (first_angle, second_angle, yaw)
    if mask & FIELD_TEMP:
        temp = struct.unpack_from("<h", frame, offset)[0] / TEMP_LSB_PER_DEGC; offset += 2
    if mask & FIELD_RAW_ACCEL:
        raw_accel = struct.unpack_from("<fff", frame, offset); offset += 12
    if mask & FIELD_RAW_GYRO:
        raw_gyro = struct.unpack_from("<fff", frame, offset); offset += 12
    if mask & FIELD_QUATERNION:
        quaternion = struct.unpack_from("<ffff", frame, offset)

    return G356Data(
        valid_fields=mask,
        accel=accel,
        gyro=gyro,
        euler=euler,
        temp=temp,
        raw_accel=raw_accel,
        raw_gyro=raw_gyro,
        quaternion=quaternion,
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
                euler_text = "euler=--" if data.euler is None else "roll={:8.2f} pitch={:8.2f} yaw={:8.2f}".format(*data.euler)
                accel_text = "acc=--" if data.accel is None else "acc=[{:7.3f},{:7.3f},{:7.3f}]g".format(*data.accel)
                gyro_text = "gyro=--" if data.gyro is None else "gyro=[{:8.2f},{:8.2f},{:8.2f}]dps".format(*data.gyro)
                temp_text = "temp=--" if data.temp is None else f"temp={data.temp:5.1f}C"
                quaternion_text = "q=--" if data.quaternion is None else "q=[{:.5f},{:.5f},{:.5f},{:.5f}]".format(*data.quaternion)
                print(
                    f"{euler_text} {accel_text} {gyro_text} {temp_text} {quaternion_text} "
                    f"valid={valid_count} invalid={invalid_count} fps={fps:5.1f}"
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
