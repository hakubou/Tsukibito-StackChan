#!/usr/bin/env python3
"""付き人スタックチャンへの中継（ゲームのパソコンで動かす）

印刷プログラム（市橋さんの heian リポジトリの tools/print_server.py）は、結末で
  POST <スタックチャンの住所>/event  {"type":"game_end","verdict":"kokoro","sent":"…","reply":"…"}
を送ってくる。これをこのパソコンの中（127.0.0.1:50090）で受けて、USB でつないだスタックチャンに
  {"type":"game_end","verdict":"kokoro"}
の 1 行にして送る（Wi-Fi を使わないので、テザリングの具合や名前探しに左右されない）。

使い方（Windows）:
  1. スタックチャンをこのパソコンに USB でつなぐ
  2. tools\\start_stackchan_bridge.bat をダブルクリック（初回は pyserial を入れる）
  3. 印刷プログラムを、知らせ先をここにして起動する
       python print_server.py --stackchan http://127.0.0.1:50090/event
  試し: python stackchan_bridge.py --test kokoro   （スタックチャンが結末のセリフを話せば OK）
       python print_server.py --stackchan http://127.0.0.1:50090/event --stackchan-test kokoro   （印刷プログラムから通しで）

オプション:
  --port COM5     ポートを指定する（省略すると Espressif の USB〔VID 303A〕を自動で探す）
  --listen 50090  待ち受けるポート番号
  --log           スタックチャンの出力を全部表示する（省略時は 2 秒ごとの計測の行を省く）

game_start（遊び始め）も同じように送れば、スタックチャンは遊んでいる間の呼び込みに切り替える。
"""
import argparse
import json
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("pyserial がありません。次を実行してから、もう一度起動してください：\n  python -m pip install pyserial")
    sys.exit(1)

ESPRESSIF_VID = 0x303A
KNOWN_TYPES = ("game_start", "game_end")
KNOWN_VERDICTS = ("kokoro", "tashinami", "nashi")


def log(msg):
    print(time.strftime("%H:%M:%S"), msg, flush=True)


class Link:
    """スタックチャンとの USB シリアル。抜けても自動でつなぎ直す"""

    def __init__(self, port, show_all):
        self.port_arg = port
        self.show_all = show_all
        self.ser = None
        self.lock = threading.Lock()
        threading.Thread(target=self._run, daemon=True).start()

    def find_port(self):
        if self.port_arg:
            return self.port_arg
        for p in list_ports.comports():
            if p.vid == ESPRESSIF_VID:
                return p.device
        return None

    def _open(self, name):
        if "://" in name:  # loop:// など（動作確認用）
            return serial.serial_for_url(name, baudrate=115200, timeout=0.2)
        s = serial.Serial()
        s.port = name
        s.baudrate = 115200
        s.timeout = 0.2
        s.dtr = False  # 開いたときにリセットがかからないよう、DTR/RTS は動かさない
        s.rts = False
        s.open()
        return s

    def _run(self):
        warned = False
        buf = b""
        while True:
            if self.ser is None:
                name = self.find_port()
                if not name:
                    if not warned:
                        log("スタックチャンが見つかりません（USB をつないでください）。見つかるまで待ちます…")
                        warned = True
                    time.sleep(2)
                    continue
                try:
                    with self.lock:
                        self.ser = self._open(name)
                    log(f"スタックチャンにつながりました（{name}）")
                    warned = False
                except Exception as e:
                    if not warned:
                        log(f"{name} を開けません：{e}（ほかのソフトが使っていないか確認）。待ちます…")
                        warned = True
                    time.sleep(2)
                    continue
            try:
                data = self.ser.read(256)
            except Exception as e:
                log(f"スタックチャンとの接続が切れました：{e}")
                with self.lock:
                    try:
                        self.ser.close()
                    except Exception:
                        pass
                    self.ser = None
                time.sleep(1)
                continue
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", errors="replace").strip()
                if text and (self.show_all or not text.startswith("draw ")):
                    log(f"[スタックチャン] {text}")

    def send(self, obj):
        line = json.dumps(obj, ensure_ascii=True, separators=(",", ":")) + "\n"
        with self.lock:
            if self.ser is None:
                return False
            try:
                self.ser.write(line.encode("ascii"))
                self.ser.flush()
                return True
            except Exception as e:
                log(f"送れませんでした：{e}")
                return False


def to_event(data):
    """届いた知らせを、スタックチャンに送る短い形にする（歌の本文は送らない）"""
    t = data.get("type") or "game_end"
    if t not in KNOWN_TYPES:
        raise ValueError(f"知らない type：{t}")
    ev = {"type": t}
    if t == "game_end":
        v = data.get("verdict") or "nashi"
        ev["verdict"] = v if v in KNOWN_VERDICTS else "nashi"
    return ev


def make_handler(link):
    class Handler(BaseHTTPRequestHandler):
        def _reply(self, code, obj):
            body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if self.path.startswith("/ping"):
                self._reply(200, {"name": "stackchan_bridge", "connected": link.ser is not None})
            else:
                self._reply(404, {"error": "not found"})

        def do_POST(self):
            if not self.path.startswith("/event"):
                return self._reply(404, {"error": "not found"})
            try:
                n = int(self.headers.get("Content-Length", "0"))
                ev = to_event(json.loads(self.rfile.read(n).decode("utf-8")))
            except Exception as e:
                log(f"読めない知らせ：{e}")
                return self._reply(400, {"ok": False, "error": str(e)})
            if link.send(ev):
                log(f"知らせを送りました：{ev}")
                self._reply(200, {"ok": True})
            else:
                log(f"スタックチャンにつながっていないので送れませんでした：{ev}")
                self._reply(503, {"ok": False, "error": "stackchan not connected"})

        def log_message(self, *a):
            pass

    return Handler


def main():
    try:
        sys.stdout.reconfigure(errors="replace")
    except Exception:
        pass
    ap = argparse.ArgumentParser()
    ap.add_argument("--port")
    ap.add_argument("--listen", type=int, default=50090)
    ap.add_argument("--log", action="store_true")
    ap.add_argument("--test", choices=KNOWN_VERDICTS + ("start",))
    a = ap.parse_args()

    link = Link(a.port, a.log)
    if a.test:
        for _ in range(50):  # つながるまで最大 10 秒待つ
            if link.ser is not None:
                break
            time.sleep(0.2)
        ev = {"type": "game_start"} if a.test == "start" else {"type": "game_end", "verdict": a.test}
        ok = link.send(ev)
        log(f"試し送り：{ev} {'OK' if ok else '失敗（つながっていない）'}")
        time.sleep(4)  # スタックチャンの返事（event: … / speak: …）を表示する
        return 0 if ok else 1

    srv = ThreadingHTTPServer(("127.0.0.1", a.listen), make_handler(link))
    log(f"中継を始めました：http://127.0.0.1:{a.listen}/event で待っています（この窓は開いたままに）")
    log(f"印刷プログラムは --stackchan http://127.0.0.1:{a.listen}/event で起動してください")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
