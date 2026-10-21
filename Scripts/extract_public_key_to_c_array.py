#!/usr/bin/env python3
"""
公钥提取工具
将PEM格式的公钥转换为C数组
"""

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.backends import default_backend

def extract_public_key_to_c_array(pem_path, output_path):
    """提取公钥并转换为C数组"""
    # 读取公钥
    with open(pem_path, 'rb') as f:
        public_key = serialization.load_pem_public_key(
            f.read(),
            backend=default_backend()
        )

    # 导出为DER格式（二进制）
    public_key_der = public_key.public_bytes(
        encoding=serialization.Encoding.DER,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )

    # 转换为C数组
    c_array = "const uint8_t public_key[] = {\n    "
    for i, byte in enumerate(public_key_der):
        c_array += f"0x{byte:02X}"
        if i < len(public_key_der) - 1:
            c_array += ", "
        if (i + 1) % 12 == 0:
            c_array += "\n    "
    c_array += "\n};\n"
    c_array += f"const uint32_t public_key_len = {len(public_key_der)};\n"

    # 保存到文件
    with open(output_path, 'w') as f:
        f.write(c_array)

    print(f"Public key extracted to {output_path}")
    print(f"Key size: {len(public_key_der)} bytes")

# 使用示例
if __name__ == '__main__':
    extract_public_key_to_c_array('public_key.pem', 'public_key.h')