from __future__ import annotations

import hashlib
import threading
from pathlib import Path

from flask import Flask, jsonify, render_template, request

from .protocol import IAPUploader, available_ports, crc32_iso, MAX_FIRMWARE_SIZE

app = Flask(__name__)
state_lock = threading.Lock()
state = {"status": "idle", "progress": 0, "message": "等待操作", "logs": [], "file": None, "crc32": None, "connected": False, "port": None, "jump_ok": None}
uploader: IAPUploader | None = None


def log(message: str) -> None:
    with state_lock:
        state["message"] = message
        state["logs"].append(message)
        state["logs"] = state["logs"][-200:]


def public_state() -> dict:
    with state_lock:
        result = dict(state, logs=list(state["logs"]))
        result.pop("firmware", None)
        return result


@app.get("/")
def index():
    return render_template("index.html", max_size=MAX_FIRMWARE_SIZE)


@app.get("/api/ports")
def ports():
    return jsonify({"ports": available_ports()})


@app.post("/api/connect")
def connect():
    global uploader
    port = (request.json or {}).get("port")
    if not port:
        return jsonify({"error": "请选择串口"}), 400
    try:
        new_uploader = IAPUploader(port)
        with state_lock:
            uploader = new_uploader
            state.update(connected=True, port=port)
        log(f"已连接 {port} @ 115200")
        return jsonify(public_state())
    except Exception as exc:
        return jsonify({"error": f"连接失败：{exc}"}), 400


@app.post("/api/disconnect")
def disconnect():
    global uploader
    with state_lock:
        current = uploader
        uploader = None
        state.update(connected=False, port=None)
    if current:
        current.close()
    log("串口已断开")
    return jsonify(public_state())


@app.post("/api/upload")
def select_firmware():
    upload = request.files.get("file")
    if not upload or not upload.filename:
        return jsonify({"error": "请选择 .bin 固件"}), 400
    if Path(upload.filename).suffix.lower() != ".bin":
        return jsonify({"error": "仅允许选择 .bin 文件，不允许 .elf"}), 400
    data = upload.read()
    if len(data) == 0 or len(data) > MAX_FIRMWARE_SIZE:
        return jsonify({"error": f"文件大小必须在 1 到 {MAX_FIRMWARE_SIZE // 1024} KiB 之间"}), 400
    with state_lock:
        state.update(file=upload.filename, size=len(data), crc32=f"{crc32_iso(data):08X}", firmware=data)
    log(f"已载入 {upload.filename}（{len(data):,} bytes）")
    result = public_state()
    result.pop("firmware", None)
    return jsonify(result)


@app.post("/api/start")
def start():
    with state_lock:
        if state["status"] == "running":
            return jsonify({"error": "升级正在进行中"}), 409
        current = uploader
        firmware = state.get("firmware")
        state.update(status="running", progress=0, jump_ok=None)
    if current is None or firmware is None:
        return jsonify({"error": "请先连接串口并选择固件"}), 400

    def worker():
        try:
            result = current.upload(firmware, lambda p, s, m: update_progress(p, s, m))
            with state_lock:
                state.update(status="success", progress=100, jump_ok=result["jump_ok"])
            log("升级流程完成：END 成功；JUMP 结果已单独记录")
        except Exception as exc:
            with state_lock:
                state.update(status="failed")
            log(f"升级失败：{exc}")

    threading.Thread(target=worker, name="iap-upgrade", daemon=True).start()
    return jsonify(public_state())


def update_progress(progress: int, status: str, message: str) -> None:
    with state_lock:
        state.update(progress=progress, status_text=status)
    log(message)


@app.get("/api/status")
def status():
    result = public_state()
    result.pop("firmware", None)
    return jsonify(result)


if __name__ == "__main__":
    app.run(host="127.0.0.1", port=5000, debug=False)
