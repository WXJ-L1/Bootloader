from __future__ import annotations

import struct
import threading
import time
from pathlib import Path
from typing import Callable

import serial
from serial.tools import list_ports

CMD_START = 0x01
CMD_SEND_DATA = 0x02
CMD_END = 0x03
CMD_JUMP = 0x06
RESP_OK = 0x00
RESP_ERROR = 0x01
RESP_CRC_ERROR = 0x02
RESP_TIMEOUT = 0x03
RESP_FLASH_ERROR = 0x04
SOF = 0xAA
EOF = 0x55
CHUNK_SIZE = 1024
MAX_FIRMWARE_SIZE = 0x70000  # same limit enforced by Core/Src/iap.c


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def crc32_iso(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ 0xEDB88320 if crc & 1 else crc >> 1
    return (~crc) & 0xFFFFFFFF


def available_ports() -> list[dict]:
    return [{"device": p.device, "description": p.description, "hwid": p.hwid}
            for p in list_ports.comports()]


class IAPUploader:
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 2.0):
        self.ser = serial.Serial(port=port, baudrate=baudrate, timeout=timeout)
        self._io_lock = threading.Lock()

    @property
    def port(self) -> str:
        return self.ser.port

    def close(self) -> None:
        if self.ser.is_open:
            self.ser.close()

    def _packet(self, cmd: int, data: bytes = b"") -> bytes:
        body = struct.pack("<BH", cmd, len(data)) + data
        return bytes([SOF]) + body + struct.pack("<H", crc16_modbus(body)) + bytes([EOF])

    def _response(self) -> tuple[int, int]:
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            byte = self.ser.read(1)
            if byte == bytes([SOF]):
                break
        else:
            raise TimeoutError("等待 STM32 响应超时")
        header = self.ser.read(3)
        if len(header) != 3:
            raise TimeoutError("响应头不完整")
        cmd, length = struct.unpack("<BH", header)
        data = self.ser.read(length)
        crc_bytes = self.ser.read(2)
        eof = self.ser.read(1)
        if len(data) != length or len(crc_bytes) != 2 or eof != bytes([EOF]):
            raise IOError("STM32 响应帧不完整")
        received_crc = struct.unpack("<H", crc_bytes)[0]
        if crc16_modbus(header + data) != received_crc:
            raise IOError("STM32 响应 CRC16 校验失败")
        if not data:
            raise IOError("STM32 响应缺少状态码")
        return cmd, data[0]

    def command(self, cmd: int, data: bytes = b"") -> tuple[int, int]:
        with self._io_lock:
            if not self.ser.is_open:
                raise IOError("串口已断开")
            self.ser.reset_input_buffer()
            self.ser.write(self._packet(cmd, data))
            self.ser.flush()
            return self._response()

    def upload(self, firmware: bytes, progress: Callable[[int, str, str], None]) -> dict:
        if not firmware:
            raise ValueError("固件文件为空")
        if len(firmware) > MAX_FIRMWARE_SIZE:
            raise ValueError(f"固件过大，最大允许 {MAX_FIRMWARE_SIZE // 1024} KiB")
        digest = crc32_iso(firmware)
        progress(0, "准备中", f"固件大小 {len(firmware):,} bytes，CRC32 0x{digest:08X}")
        _, status = self.command(CMD_START, struct.pack("<II", len(firmware), digest))
        if status != RESP_OK:
            raise RuntimeError(f"START 失败，响应码 0x{status:02X}")
        progress(0, "擦除中", "STM32 已确认开始升级")
        total = (len(firmware) + CHUNK_SIZE - 1) // CHUNK_SIZE
        for index in range(total):
            chunk = firmware[index * CHUNK_SIZE:(index + 1) * CHUNK_SIZE]
            _, status = self.command(CMD_SEND_DATA, chunk)
            if status != RESP_OK:
                raise RuntimeError(f"数据包 {index + 1}/{total} 失败，响应码 0x{status:02X}")
            progress((index + 1) * 100 // total, "传输中", f"已确认 {index + 1}/{total} 个数据包")
        progress(100, "校验中", "数据传输完成，等待 STM32 CRC32 校验")
        _, status = self.command(CMD_END)
        if status != RESP_OK:
            reason = "CRC32 校验失败" if status == RESP_CRC_ERROR else f"响应码 0x{status:02X}"
            raise RuntimeError(f"END 失败：{reason}")
        progress(100, "完成", "END 已成功确认，固件升级成功")
        jump_cmd, jump_status = self.command(CMD_JUMP)
        jump_ok = jump_cmd == CMD_JUMP and jump_status == RESP_OK
        progress(100, "完成", "JUMP 已确认" if jump_ok else f"JUMP 响应码 0x{jump_status:02X}")
        return {"end_ok": True, "jump_ok": jump_ok, "jump_status": jump_status}
