import serial
import struct
import time
import sys

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

class IAPUploader:
    def __init__(self, port, baudrate=115200):
        self.ser = serial.Serial(port, baudrate, timeout=2)

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

        if len(eof) == 0 or eof[0] != PACKET_EOF:
            return None

        return {'cmd': cmd, 'data': data}

    def upload_firmware(self, firmware_path):
        """上传固件"""
        # 读取固件文件
        with open(firmware_path, 'rb') as f:
            firmware = f.read()

        print(f"Firmware size: {len(firmware)} bytes")

        # 计算CRC32
        crc32 = self.crc32(firmware)
        print(f"Firmware CRC32: 0x{crc32:08X}")

        # 发送开始升级命令
        print("Sending start upgrade command...")
        start_data = struct.pack('<II', len(firmware), crc32)
        self.send_packet(CMD_START_UPGRADE, start_data)

        response = self.receive_response()
        if response is None or response['data'][0] != RESP_OK:
            print("Start upgrade failed!")
            return False

        print("Start upgrade OK")

        # 分包发送固件数据
        chunk_size = 1024
        total_chunks = (len(firmware) + chunk_size - 1) // chunk_size

        for i in range(total_chunks):
            start = i * chunk_size
            end = min(start + chunk_size, len(firmware))
            chunk = firmware[start:end]

            self.send_packet(CMD_SEND_DATA, chunk)

            response = self.receive_response()
            if response is None or response['data'][0] != RESP_OK:
                print(f"Send data failed at chunk {i}")
                return False

            progress = (i + 1) * 100 // total_chunks
            print(f"Progress: {progress}%", end='\r')

        print("\nAll data sent")

        # 发送结束升级命令
        print("Sending end upgrade command...")
        self.send_packet(CMD_END_UPGRADE)

        response = self.receive_response()
        if response is None or response['data'][0] != RESP_OK:
            print("End upgrade failed!")
            return False

        print("Upgrade complete!")

        # 发送跳转命令
        print("Sending jump to app command...")
        self.send_packet(CMD_JUMP_TO_APP)

        return True

# 使用示例
if __name__ == '__main__':
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