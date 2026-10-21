import serial
import struct
import time
import sys
<<<<<<< HEAD
from pathlib import Path
=======
>>>>>>> 79ab045 (APP)

# 命令定义
CMD_START_UPGRADE = 0x01
CMD_SEND_DATA = 0x02
CMD_END_UPGRADE = 0x03
CMD_JUMP_TO_APP = 0x06

# 响应码
RESP_OK = 0x00
RESP_ERROR = 0x01
RESP_CRC_ERROR = 0x02

# 数据包格式
PACKET_SOF = 0xAA
PACKET_EOF = 0x55
<<<<<<< HEAD
DEFAULT_FIRMWARE = Path(r"D:\STM32F407\LCD\build\Debug\LCD.bin")
CHUNK_SIZE = 1024

class IAPUploader:
    def __init__(self, port, baudrate=115200):
        # 开始升级时 Bootloader 会擦除目标 Bank 的 3 个 Flash 扇区，
        # 擦除完成后才返回响应。3 秒在某些芯片/供电条件下不够。
        self.ser = serial.Serial(port, baudrate, timeout=12)
=======

class IAPUploader:
    def __init__(self, port, baudrate=115200):
        self.ser = serial.Serial(port, baudrate, timeout=2)
>>>>>>> 79ab045 (APP)

    def crc16(self, data):
        """计算CRC16"""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 0x0001:
                    crc = (crc >> 1) ^ 0xA001
                else:
                    crc = crc >> 1
        return crc

    def crc32(self, data):
        """计算CRC32"""
        crc = 0xFFFFFFFF
        for byte in data:
            crc ^= byte
            for _ in range(8):
                if crc & 1:
                    crc = (crc >> 1) ^ 0xEDB88320
                else:
                    crc = crc >> 1
        return ~crc & 0xFFFFFFFF

    def send_packet(self, cmd, data=b''):
        """发送数据包"""
        # 构建数据包
        length = len(data)
        packet = struct.pack('<BBH', PACKET_SOF, cmd, length)
        packet += data

        # 计算CRC
        crc = self.crc16(packet[1:])
        packet += struct.pack('<H', crc)
        packet += struct.pack('B', PACKET_EOF)

        # 发送
        self.ser.write(packet)

    def receive_response(self):
        """接收响应"""
        # 等待SOF
        while True:
            byte = self.ser.read(1)
            if len(byte) == 0:
                return None
            if byte[0] == PACKET_SOF:
                break

        # 接收剩余数据
        header = self.ser.read(3)  # CMD + LEN
        if len(header) != 3:
            return None

        cmd = header[0]
        length = struct.unpack('<H', header[1:3])[0]

        data = self.ser.read(length)
        crc_bytes = self.ser.read(2)
        eof = self.ser.read(1)

<<<<<<< HEAD
        if len(data) != length or len(crc_bytes) != 2:
            return None
        if len(eof) == 0 or eof[0] != PACKET_EOF:
            return None

        received_crc = struct.unpack('<H', crc_bytes)[0]
        calculated_crc = self.crc16(bytes([cmd]) + header[1:3] + data)
        if calculated_crc != received_crc:
            return None

        return {'cmd': cmd, 'data': data}

    def wait_response(self, expected_cmd):
        response = self.receive_response()
        if response is None:
            raise RuntimeError("等待 Bootloader 响应超时或响应 CRC 错误")
        if response['cmd'] != expected_cmd:
            raise RuntimeError(
                f"Bootloader 响应命令错误：期望 0x{expected_cmd:02X}，"
                f"实际 0x{response['cmd']:02X}"
            )
        if not response['data'] or response['data'][0] != RESP_OK:
            status = response['data'][0] if response['data'] else 0xFF
            raise RuntimeError(f"Bootloader 响应失败，状态码 0x{status:02X}")
        return response

    def send_and_wait(self, cmd, data=b'', retries=3):
        last_error = None
        for attempt in range(1, retries + 1):
            try:
                self.ser.reset_input_buffer()
                self.send_packet(cmd, data)
                return self.wait_response(cmd)
            except RuntimeError as exc:
                last_error = exc
                if attempt < retries:
                    print(f"等待命令 0x{cmd:02X} 响应失败，第 {attempt} 次重试...")
                    time.sleep(0.2)
        raise last_error

    def upload_firmware(self, firmware_path):
        """上传固件"""
        # 读取固件文件
        firmware_path = Path(firmware_path).resolve()
        if not firmware_path.is_file():
            raise FileNotFoundError(f"找不到固件文件：{firmware_path}")

        with firmware_path.open('rb') as f:
            firmware = f.read()

        print(f"Firmware file: {firmware_path}")
=======
        if len(eof) == 0 or eof[0] != PACKET_EOF:
            return None

        return {'cmd': cmd, 'data': data}

    def upload_firmware(self, firmware_path):
        """上传固件"""
        # 读取固件文件
        with open(firmware_path, 'rb') as f:
            firmware = f.read()

>>>>>>> 79ab045 (APP)
        print(f"Firmware size: {len(firmware)} bytes")

        # 计算CRC32
        crc32 = self.crc32(firmware)
        print(f"Firmware CRC32: 0x{crc32:08X}")

        # 发送开始升级命令
<<<<<<< HEAD
        print("Waiting for Bootloader to be ready...")
        time.sleep(2.2)
        self.ser.reset_input_buffer()
        print("Sending start upgrade command...")
        start_data = struct.pack('<II', len(firmware), crc32)
        # START 会触发整块 Bank 擦除，允许更长时间完成。
        self.send_and_wait(CMD_START_UPGRADE, start_data, retries=1)
=======
        print("Sending start upgrade command...")
        start_data = struct.pack('<II', len(firmware), crc32)
        self.send_packet(CMD_START_UPGRADE, start_data)

        response = self.receive_response()
        if response is None or response['data'][0] != RESP_OK:
            print("Start upgrade failed!")
            return False
>>>>>>> 79ab045 (APP)

        print("Start upgrade OK")

        # 分包发送固件数据
<<<<<<< HEAD
        chunk_size = CHUNK_SIZE
=======
        chunk_size = 1024
>>>>>>> 79ab045 (APP)
        total_chunks = (len(firmware) + chunk_size - 1) // chunk_size

        for i in range(total_chunks):
            start = i * chunk_size
            end = min(start + chunk_size, len(firmware))
            chunk = firmware[start:end]

<<<<<<< HEAD
            try:
                self.send_and_wait(CMD_SEND_DATA, chunk, retries=1)
            except RuntimeError as exc:
                print(f"Send data failed at chunk {i}: {exc}")
=======
            self.send_packet(CMD_SEND_DATA, chunk)

            response = self.receive_response()
            if response is None or response['data'][0] != RESP_OK:
                print(f"Send data failed at chunk {i}")
>>>>>>> 79ab045 (APP)
                return False

            progress = (i + 1) * 100 // total_chunks
            print(f"Progress: {progress}%", end='\r')

        print("\nAll data sent")

        # 发送结束升级命令
        print("Sending end upgrade command...")
<<<<<<< HEAD
        try:
            self.send_and_wait(CMD_END_UPGRADE, retries=1)
        except RuntimeError as exc:
            print(f"End upgrade failed: {exc}")
=======
        self.send_packet(CMD_END_UPGRADE)

        response = self.receive_response()
        if response is None or response['data'][0] != RESP_OK:
            print("End upgrade failed!")
>>>>>>> 79ab045 (APP)
            return False

        print("Upgrade complete!")

        # 发送跳转命令
        print("Sending jump to app command...")
<<<<<<< HEAD
        try:
            self.send_and_wait(CMD_JUMP_TO_APP, retries=1)
        except RuntimeError as exc:
            print(f"Jump to app failed: {exc}")
            return False
=======
        self.send_packet(CMD_JUMP_TO_APP)
>>>>>>> 79ab045 (APP)

        return True

# 使用示例
if __name__ == '__main__':
<<<<<<< HEAD
    if len(sys.argv) not in (2, 3):
        print("Usage: python iap_uploader.py <COM_PORT> [FIRMWARE_FILE]")
        print(f"Default firmware: {DEFAULT_FIRMWARE}")
        sys.exit(1)

    port = sys.argv[1]
    firmware_file = sys.argv[2] if len(sys.argv) == 3 else DEFAULT_FIRMWARE

    uploader = None
    try:
        uploader = IAPUploader(port)

        if uploader.upload_firmware(firmware_file):
            print("Firmware upload successful!")
        else:
            print("Firmware upload failed!")
    except Exception as exc:
        print(f"Firmware upload failed: {exc}")
        sys.exit(1)
    finally:
        if uploader is not None:
            uploader.ser.close()
=======
    if len(sys.argv) != 3:
        print("Usage: python iap_uploader.py <COM_PORT> <FIRMWARE_FILE>")
        sys.exit(1)

    port = sys.argv[1]
    firmware_file = sys.argv[2]

    uploader = IAPUploader(port)

    if uploader.upload_firmware(firmware_file):
        print("Firmware upload successful!")
    else:
        print("Firmware upload failed!")
>>>>>>> 79ab045 (APP)
