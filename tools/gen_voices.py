#!/usr/bin/env python3
"""voice/phrases.json のセリフを VOICEVOX で音声化し、ファームウェアに埋め込む形式で出力する。

出力:
  assets/voices.bin   … 全セリフの PCM（16bit モノラル）を連結したもの
  src/voice_table.h   … 各セリフの位置・長さ・字幕・表情の表

使い方:
  docker run --rm -d -p 127.0.0.1:50021:50021 --name voicevox voicevox/voicevox_engine:cpu-latest
  python3 tools/gen_voices.py
"""
import io
import json
import sys
import urllib.parse
import urllib.request
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENGINE = "http://127.0.0.1:50021"


def post(path, params, body=None):
    url = f"{ENGINE}{path}?{urllib.parse.urlencode(params)}"
    data = json.dumps(body).encode() if body is not None else b""
    req = urllib.request.Request(url, data=data, method="POST",
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=120) as r:
        return r.read()


def synth(text, cfg):
    query = json.loads(post("/audio_query", {"text": text, "speaker": cfg["speaker"]}))
    query["outputSamplingRate"] = cfg["sample_rate"]
    query["outputStereo"] = False
    query["speedScale"] = cfg.get("speed", 1.0)
    wav = post("/synthesis", {"speaker": cfg["speaker"]}, query)
    with wave.open(io.BytesIO(wav)) as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2, "16bit mono 以外"
        assert w.getframerate() == cfg["sample_rate"], w.getframerate()
        return w.readframes(w.getnframes())


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main():
    cfg = json.loads((ROOT / "voice/phrases.json").read_text(encoding="utf-8"))
    blob = bytearray()
    rows = []
    for p in cfg["phrases"]:
        pcm = synth(p["text"], cfg)
        rows.append((p, len(blob) // 2, len(pcm) // 2))
        blob += pcm
        print(f'{p["id"]:10s} {len(pcm) / 2 / cfg["sample_rate"]:5.2f}s  {p["text"]}')

    (ROOT / "assets").mkdir(exist_ok=True)
    (ROOT / "assets/voices.bin").write_bytes(blob)

    lines = [
        "// tools/gen_voices.py が自動生成。手で編集しないこと",
        f'// 音声: VOICEVOX:{cfg["speaker_name"]}',
        "#pragma once",
        "#include \"HeianFace.h\"",
        "",
        f"constexpr uint32_t kVoiceSampleRate = {cfg['sample_rate']};",
        "",
        "struct VoiceClip {",
        "  const char* id;",
        "  const char* caption;",
        "  heian::Expr expression;",
        "  uint32_t offset;   // voices.bin 先頭からのサンプル位置",
        "  uint32_t samples;  // サンプル数",
        "};",
        "",
        "const VoiceClip kVoiceClips[] = {",
    ]
    for p, off, n in rows:
        lines.append(f'  {{{c_str(p["id"])}, {c_str(p["caption"])}, '
                     f'heian::Expr::{p["expression"]}, {off}, {n}}},')
    lines += ["};", "constexpr int kVoiceClipCount = sizeof(kVoiceClips) / sizeof(kVoiceClips[0]);", ""]
    (ROOT / "src/voice_table.h").write_text("\n".join(lines), encoding="utf-8")
    print(f"total {len(blob) / 1024:.0f} KB -> assets/voices.bin, src/voice_table.h")


if __name__ == "__main__":
    sys.exit(main())
