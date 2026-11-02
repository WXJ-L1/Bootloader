# STM32F407 UART IAP Web 升级控制台
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
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python run_web.py
```
