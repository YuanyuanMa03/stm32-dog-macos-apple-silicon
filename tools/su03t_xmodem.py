"""Minimal XMODEM-1K/CRC sender for SU-03T serial update.

The local vendor updater logs show 921600 8N1 and three receiver 'C' bytes.
Its packet builder uses 1024-byte blocks, 0x1A padding and CRC-16/XMODEM.
This module keeps the protocol separate from pyserial so it can be tested.
"""

from __future__ import annotations

import binascii
import math
import time
from typing import BinaryIO, Callable, Protocol

STX = b"\x02"
EOT = b"\x04"
ACK = b"\x06"
NAK = b"\x15"
CAN = b"\x18"
CRC_REQUEST = b"C"
BLOCK_SIZE = 1024


class SerialPort(Protocol):
    timeout: float | None

    def read(self, size: int = 1) -> bytes: ...

    def write(self, data: bytes) -> int: ...

    def flush(self) -> None: ...


class TransferError(RuntimeError):
    pass


def wait_for_handshake(port: SerialPort, seconds: float) -> None:
    """Wait for the three consecutive 'C' bytes logged by the vendor tool.

    No bytes are written to the serial port before this function returns.
    """
    deadline = time.monotonic() + seconds
    consecutive = 0
    while time.monotonic() < deadline:
        port.timeout = min(0.25, max(0.01, deadline - time.monotonic()))
        byte = port.read(1)
        if byte == CRC_REQUEST:
            consecutive += 1
            if consecutive == 3:
                return
        elif byte:
            consecutive = 0
    raise TransferError("未收到连续 CCC 升级握手；没有发送任何固件数据")


def _packet(sequence: int, data: bytes) -> bytes:
    if not 1 <= len(data) <= BLOCK_SIZE:
        raise ValueError("XMODEM 数据块长度必须为 1..1024")
    padded = data.ljust(BLOCK_SIZE, b"\x1a")
    crc = binascii.crc_hqx(padded, 0)
    return STX + bytes((sequence & 0xFF, 0xFF - (sequence & 0xFF))) + padded + crc.to_bytes(2, "big")


def _write_all(port: SerialPort, data: bytes) -> None:
    written = port.write(data)
    if written != len(data):
        raise TransferError(f"串口只发送 {written}/{len(data)} 字节")
    port.flush()


def _reply(port: SerialPort, seconds: float) -> bytes | None:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        port.timeout = min(0.25, max(0.01, deadline - time.monotonic()))
        byte = port.read(1)
        if byte in (ACK, NAK, CAN):
            return byte
        # A leftover 'C' from the startup sequence is not an ACK or failure.
    return None


def send_xmodem_1k(
    port: SerialPort,
    source: BinaryIO,
    size: int,
    *,
    retries: int = 16,
    response_timeout: float = 5.0,
    progress: Callable[[int, int], None] | None = None,
) -> int:
    """Send after handshake; return acknowledged block count after EOT ACK."""
    if size <= 0:
        raise ValueError("固件不能为空")
    count = math.ceil(size / BLOCK_SIZE)
    for index in range(count):
        data = source.read(BLOCK_SIZE)
        if not data:
            raise TransferError("固件读取提前结束")
        frame = _packet((index + 1) & 0xFF, data)
        for _ in range(retries + 1):
            _write_all(port, frame)
            reply = _reply(port, response_timeout)
            if reply == ACK:
                if progress:
                    progress(index + 1, count)
                break
            if reply == CAN:
                raise TransferError(f"模块取消了第 {index + 1} 块传输")
        else:
            raise TransferError(f"第 {index + 1}/{count} 块未得到 ACK")

    for _ in range(retries + 1):
        _write_all(port, EOT)
        reply = _reply(port, response_timeout)
        if reply == ACK:
            return count
        if reply == CAN:
            raise TransferError("模块取消了传输结束确认")
    raise TransferError("EOT 未得到 ACK；不能认定升级完成")
