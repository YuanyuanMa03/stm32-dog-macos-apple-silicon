import binascii
from collections import deque
from io import BytesIO
import unittest

from tools.su03t_xmodem import (
    ACK,
    BLOCK_SIZE,
    EOT,
    STX,
    TransferError,
    send_xmodem_1k,
    wait_for_handshake,
)


class FakeReceiver:
    timeout = 0.01

    def __init__(self, incoming=b"CCC", nak_first=False):
        self.incoming = deque(bytes((byte,)) for byte in incoming)
        self.writes = []
        self.nak_first = nak_first
        self.blocks = 0

    def read(self, size=1):
        return self.incoming.popleft() if self.incoming else b""

    def write(self, data):
        self.writes.append(data)
        if data == EOT:
            self.incoming.append(ACK)
        elif data[0:1] == STX:
            self.blocks += 1
            if self.nak_first and self.blocks == 1:
                self.incoming.append(b"\x15")
            else:
                self.incoming.append(ACK)
        return len(data)

    def flush(self):
        pass


class XmodemTests(unittest.TestCase):
    def test_no_handshake_never_writes(self):
        receiver = FakeReceiver(incoming=b"\x00")
        with self.assertRaisesRegex(TransferError, "没有发送任何固件数据"):
            wait_for_handshake(receiver, 0.01)
        self.assertEqual(receiver.writes, [])

    def test_packet_crc_padding_and_eot(self):
        receiver = FakeReceiver()
        wait_for_handshake(receiver, 0.01)
        payload = b"example firmware"
        count = send_xmodem_1k(receiver, BytesIO(payload), len(payload))
        self.assertEqual(count, 1)
        frame = receiver.writes[0]
        self.assertEqual(frame[:3], STX + b"\x01\xfe")
        self.assertEqual(frame[3 : 3 + len(payload)], payload)
        self.assertEqual(frame[3 + len(payload) : 3 + BLOCK_SIZE], b"\x1a" * (BLOCK_SIZE - len(payload)))
        self.assertEqual(frame[-2:], binascii.crc_hqx(frame[3:-2], 0).to_bytes(2, "big"))
        self.assertEqual(receiver.writes[-1], EOT)

    def test_nak_retries_same_packet(self):
        receiver = FakeReceiver(nak_first=True)
        wait_for_handshake(receiver, 0.01)
        count = send_xmodem_1k(receiver, BytesIO(b"data"), 4)
        self.assertEqual(count, 1)
        self.assertEqual(receiver.writes[0], receiver.writes[1])

    def test_sequence_wraps_after_255(self):
        receiver = FakeReceiver()
        wait_for_handshake(receiver, 0.01)
        data = b"z" * (BLOCK_SIZE * 257)
        count = send_xmodem_1k(receiver, BytesIO(data), len(data))
        self.assertEqual(count, 257)
        self.assertEqual(receiver.writes[254][1:3], b"\xff\x00")
        self.assertEqual(receiver.writes[255][1:3], b"\x00\xff")
        self.assertEqual(receiver.writes[256][1:3], b"\x01\xfe")


if __name__ == "__main__":
    unittest.main()
