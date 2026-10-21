# STM32F407 UART IAP Web 升级控制台

本项目在现有 Bootloader 协议之上增加本机 Flask Web 界面，不修改 STM32 端代码。后端复用协议常量和 CRC 算法，严格按 `START → SEND_DATA(每包等待 ACK) → END → JUMP` 执行；只有 END 返回 `RESP_OK` 才判定升级成功，JUMP 结果单独显示。

## 目录

```text
webapp/
  protocol.py        串口、帧封装、CRC、IAP 上传
  server.py          Flask REST API 与后台线程
  templates/index.html
  static/style.css
  static/app.js
run_web.py
requirements.txt
```

## Windows PowerShell 启动

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
py -m pip install -r requirements.txt
py run_web.py
```

然后访问 http://127.0.0.1:5000 。固件大小上限与 `Core/Src/iap.c` 当前检查一致，为 `0x70000`（448 KiB）。如果未来调整 STM32 端分区或上限，请同步修改 `webapp/protocol.py` 的 `MAX_FIRMWARE_SIZE`。

## 已确认的协议注意事项

现有 `sci/iap_uploader.py` 未验证响应 CRC，且发送 JUMP 后未读取响应。Web 版在自己的模块中补上了响应 CRC 校验和 JUMP ACK 读取；没有改动 Bootloader。Bootloader 的 `START` 上限是 `0x70000`，虽然 `flash.h` 中还定义了更大的 APP 区域，实际协议检查仍以前者为准。
