#!/usr/bin/env python3
"""
固件签名工具
使用RSA-2048或ECDSA-256对固件进行签名
"""

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa, ec, padding
from cryptography.hazmat.backends import default_backend
import struct
import sys

class FirmwareSigner:
    def __init__(self, algorithm='RSA'):
        self.algorithm = algorithm
        self.private_key = None
        self.public_key = None

    def generate_keypair(self):
        """生成密钥对"""
        if self.algorithm == 'RSA':
            # 生成RSA-2048密钥对
            self.private_key = rsa.generate_private_key(
                public_exponent=65537,
                key_size=2048,
                backend=default_backend()
            )
        elif self.algorithm == 'ECDSA':
            # 生成ECDSA-256密钥对
            self.private_key = ec.generate_private_key(
                ec.SECP256R1(), 
                default_backend()
            )

        self.public_key = self.private_key.public_key()

    def save_keys(self, private_path, public_path):
        """保存密钥到文件"""
        # 保存私钥（PEM格式，加密）
        private_pem = self.private_key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.PKCS8,
            encryption_algorithm=serialization.BestAvailableEncryption(b'password')
        )
        with open(private_path, 'wb') as f:
            f.write(private_pem)

        # 保存公钥（PEM格式）
        public_pem = self.public_key.public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        )
        with open(public_path, 'wb') as f:
            f.write(public_pem)

    def load_private_key(self, path, password=b'password'):
        """加载私钥"""
        with open(path, 'rb') as f:
            self.private_key = serialization.load_pem_private_key(
                f.read(),
                password=password,
                backend=default_backend()
            )

    def sign_firmware(self, firmware_path, output_path):
        """签名固件"""
        # 读取固件
        with open(firmware_path, 'rb') as f:
            firmware_data = f.read()

        # 计算固件哈希
        from cryptography.hazmat.primitives import hashes
        digest = hashes.Hash(hashes.SHA256(), backend=default_backend())
        digest.update(firmware_data)
        firmware_hash = digest.finalize()

        # 生成签名
        if self.algorithm == 'RSA':
            signature = self.private_key.sign(
                firmware_hash,
                padding.PSS(
                    mgf=padding.MGF1(hashes.SHA256()),
                    salt_length=padding.PSS.MAX_LENGTH
                ),
                hashes.SHA256()
            )
        elif self.algorithm == 'ECDSA':
            signature = self.private_key.sign(
                firmware_hash,
                ec.ECDSA(hashes.SHA256())
            )

        # 构建签名固件格式
        # [固件大小(4字节)][固件数据][签名长度(2字节)][签名数据]
        signed_firmware = struct.pack('<I', len(firmware_data))
        signed_firmware += firmware_data
        signed_firmware += struct.pack('<H', len(signature))
        signed_firmware += signature

        # 保存签名固件
        with open(output_path, 'wb') as f:
            f.write(signed_firmware)

        print(f"Firmware signed successfully!")
        print(f"Firmware size: {len(firmware_data)} bytes")
        print(f"Signature size: {len(signature)} bytes")
        print(f"Total size: {len(signed_firmware)} bytes")

# 使用示例
if __name__ == '__main__':
    signer = FirmwareSigner(algorithm='RSA')

    # 生成密钥对
    signer.generate_keypair()
    signer.save_keys('private_key.pem', 'public_key.pem')

    # 签名固件
    signer.sign_firmware('LCD.bin', 'LCD_signed.bin')