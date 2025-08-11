# gui/chat_panel.py
import asyncio
import json
from typing import Optional

from PyQt6.QtCore import QObject, QThread, pyqtSignal
from PyQt6.QtWidgets import (QHBoxLayout, QLineEdit, QPushButton, QTextEdit,
                             QVBoxLayout, QWidget)


class ChatPanel(QWidget):
    """Simple Chat panel that talks JSON lines to 127.0.0.1:5055."""

    def __init__(self, host: str = "127.0.0.1", port: int = 5055, parent: Optional[QWidget] = None):
        super().__init__(parent)
        self.host = host
        self.port = port

        self.transcript = QTextEdit()
        self.transcript.setReadOnly(True)
        self.input = QLineEdit()
        self.send_btn = QPushButton("Send")

        row = QHBoxLayout()
        row.addWidget(self.input)
        row.addWidget(self.send_btn)

        layout = QVBoxLayout(self)
        layout.addWidget(self.transcript)
        layout.addLayout(row)

        self.send_btn.clicked.connect(self.on_send)
        self.input.returnPressed.connect(self.on_send)

        # Async worker in a thread to avoid blocking Qt event loop
        self._loop = asyncio.new_event_loop()
        self._worker = _ChatWorker(self.host, self.port, self._loop)
        self._thread = QThread(self)
        self._worker.moveToThread(self._thread)
        self._thread.started.connect(self._worker.start)
        self._worker.received.connect(self._on_received)
        self._thread.start()

        # Try initial connection; worker will emit error on failure
        self._append_info("Chat connected to %s:%d (attempting)" % (self.host, self.port))

    def _append_info(self, msg: str):
        self.transcript.append(f"[Info] {msg}")

    def _append_user(self, msg: str):
        self.transcript.append(f"[Operator] {msg}")

    def _append_sim(self, msg: str):
        self.transcript.append(f"[Sim] {msg}")

    def on_send(self):
        text = self.input.text().strip()
        if not text:
            return
        self._append_user(text)
        payload = self._text_to_command(text)
        self._worker.send(json.dumps(payload) + "\n")
        self.input.clear()

    def _text_to_command(self, t: str) -> dict:
        s = t.lower()
        if s in ("status", "stat"):
            return {"cmd": "status"}
        if s in ("go", "go for launch"):
            return {"cmd": "go"}
        if s in ("nogo", "no-go"):
            return {"cmd": "nogo"}
        if s in ("abort", "e-stop"):
            return {"cmd": "abort"}
        if s.startswith("throttle "):
            try:
                val = int(s.split()[1])
            except Exception:
                val = 0
            return {"cmd": "set_throttle", "value": val}
        return {"cmd": s}

    def _on_received(self, line: str):
        self._append_sim(line)


class _ChatWorker(QObject):
    received = pyqtSignal(str)

    def __init__(self, host: str, port: int, loop: asyncio.AbstractEventLoop):
        super().__init__()
        self._host = host
        self._port = port
        self._loop = loop
        self._queue: asyncio.Queue[str] = asyncio.Queue()

    def start(self):
        asyncio.set_event_loop(self._loop)
        self._loop.create_task(self._run())
        self._loop.run_forever()

    def send(self, line: str):
        self._loop.call_soon_threadsafe(self._queue.put_nowait, line)

    async def _run(self):
        try:
            reader, writer = await asyncio.open_connection(self._host, self._port)
        except Exception as e:
            # Tell UI about connection failure
            self.received.emit(json.dumps({"type": "error", "msg": f"connect failed: {e}"}))
            return

        while True:
            # Send queued messages if any
            try:
                line = await asyncio.wait_for(self._queue.get(), timeout=0.05)
                writer.write(line.encode("utf-8"))
                await writer.drain()
            except asyncio.TimeoutError:
                pass

            # Read incoming lines
            try:
                data = await asyncio.wait_for(reader.readline(), timeout=0.05)
            except asyncio.TimeoutError:
                continue
            if not data:
                self.received.emit(json.dumps({"type": "info", "msg": "connection closed"}))
                break
            self.received.emit(data.decode("utf-8").strip())
