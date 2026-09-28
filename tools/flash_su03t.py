#!/usr/bin/env python3
"""Flash a serial-upgradable SU-03T1 from macOS, after a CCC handshake."""

from __future__ import annotations

import argparse
from hashlib import sha256
from pathlib import Path
import sys

from su03t_xmodem import TransferError, send_xmodem_1k, wait_for_handshake


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="例如 /dev/cu.wchusbserial1440")
    parser.add_argument("--firmware", required=True, type=Path, help="SU-03T1 的 *_update.bin")
    parser.add_argument("--sha256", help="可选：预期 SHA-256，用于固定资料版本")
    parser.add_argument("--handshake-timeout", type=float, default=180)
    return parser.parse_args()


def main() -> int:
    args = arguments()
    firmware: Path = args.firmware
    if not firmware.name.endswith("_update.bin"):
        print("拒绝：B6/B7 串口升级只能使用 *_update.bin", file=sys.stderr)
        return 2
    if not firmware.is_file() or firmware.stat().st_size == 0:
        print("固件文件不存在或为空", file=sys.stderr)
        return 2
    digest = sha256(firmware.read_bytes()).hexdigest()
    if args.sha256 and digest.lower() != args.sha256.lower():
        print(f"SHA-256 不匹配：{digest}", file=sys.stderr)
        return 2
    print(f"固件：{firmware.name} ({firmware.stat().st_size} 字节，SHA-256 {digest})", flush=True)

    try:
        import serial
    except ImportError:
        print("缺少 pyserial；请先安装 requirements.txt", file=sys.stderr)
        return 2

    try:
        with serial.Serial(args.port, 921600, timeout=0.25, write_timeout=10) as port:
            port.reset_input_buffer()
            print("READY：串口 921600 8N1 已打开；现在给独立 SU-03T1 接通 5V。", flush=True)
            wait_for_handshake(port, args.handshake_timeout)
            print("收到 CCC，开始发送；此时不要断电。", flush=True)

            def progress(done: int, total: int) -> None:
                if done % 100 == 0 or done == total:
                    print(f"已确认 {done}/{total} 块", flush=True)

            with firmware.open("rb") as source:
                blocks = send_xmodem_1k(
                    port, source, firmware.stat().st_size, progress=progress
                )
            print(f"EOT 已得到 ACK，共 {blocks} 块；请等待模块重启并测试语音。", flush=True)
            return 0
    except (OSError, TransferError, ValueError) as exc:
        print(f"升级未确认成功：{exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
