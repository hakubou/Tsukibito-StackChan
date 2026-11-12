#!/usr/bin/env python3
"""src/HeianFace.h と同じ座標・色で平安顔を PNG に描き出す（README 用の画面イメージ）。

出力: docs/images/face.png（通常の顔）, docs/images/face_expressions.png（表情一覧）
HeianFace.h のパーツ位置や色を変えたら、このスクリプトも合わせて直すこと。
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs/images"
SS = 4  # スーパーサンプリング倍率

GOFUN = (0xF3, 0xEE, 0xE3)
SUMI = (0x2B, 0x2B, 0x2B)
MAYU = (0x3A, 0x3A, 0x3A)
MAYU_SOFT = (0xA8, 0xA4, 0x9C)
NOSE = (0x8A, 0x7F, 0x75)
BENI = (0xB8, 0x34, 0x3F)
BENI_DARK = (0x5A, 0x10, 0x18)
HOHO = (0xEF, 0xC3, 0xC0)


def ellipse(d, cx, cy, rx, ry, color):
    d.ellipse([(cx - rx) * SS, (cy - ry) * SS, (cx + rx) * SS, (cy + ry) * SS], fill=color)


def wide_line(d, x0, y0, x1, y1, r, color):
    d.line([(x0 * SS, y0 * SS), (x1 * SS, y1 * SS)], fill=color, width=max(1, round(2 * r * SS)))
    for x, y in ((x0, y0), (x1, y1)):
        ellipse(d, x, y, r, r, color)


def curve(d, x0, y0, cx, cy, x1, y1, r, color):
    px, py = x0, y0
    for i in range(1, 13):
        t = i / 12
        u = 1 - t
        x = u * u * x0 + 2 * u * t * cx + t * t * x1
        y = u * u * y0 + 2 * u * t * cy + t * t * y1
        wide_line(d, px, py, x, y, r, color)
        px, py = x, y


def eye(d, cx, cy, is_left, exp, open_=1.0):
    outer = 1 if is_left else -1
    ellipse(d, cx + outer * 18, cy + 34, 22, 12, HOHO)
    half = 22
    y_in = y_out = cy
    bend, r = 4, 1.6
    if exp == "Happy":
        bend = -9
    elif exp == "Sad":
        y_out += 5; y_in -= 2
    elif exp == "Angry":
        y_in += 4; y_out -= 3; bend = 1
    elif exp == "Sleepy":
        y_in += 3; y_out += 3; bend = 1; r = 1.2
    elif exp == "Doubt":
        if is_left:
            y_in -= 2; y_out -= 2
        bend = 2
    x_in, x_out = cx - outer * half, cx + outer * half
    if open_ < 0.5:
        y = (y_in + y_out) / 2 + 2
        wide_line(d, x_in, y, x_out, y, 1.0, SUMI)
        return
    curve(d, x_in, y_in, cx, cy + bend * 2, x_out, y_out, r, SUMI)
    if exp not in ("Happy", "Sleepy"):
        ellipse(d, cx, (y_in + y_out) / 2 + bend + 1, 3, 2, SUMI)


def mayu(d, cx, cy, is_left, exp):
    if exp == "Happy":
        cy -= 3
    elif exp == "Sad":
        cy -= 6; cx += -3 if is_left else 3
    elif exp == "Angry":
        cy += 4; cx += -4 if is_left else 4
    elif exp == "Sleepy":
        cy += 2
    elif exp == "Doubt" and is_left:
        cy -= 6
    ellipse(d, cx, cy, 17, 11, MAYU_SOFT)
    ellipse(d, cx, cy, 12, 8, MAYU)


def kuchi(d, cx, cy, exp, open_=0.0):
    ny = cy - 34
    wide_line(d, cx + 2, ny - 7, cx - 3, ny + 1, 0.9, NOSE)
    wide_line(d, cx - 3, ny + 1, cx + 3, ny + 3, 0.9, NOSE)
    rx, ry = 6, 3.5
    if exp == "Happy":
        rx, ry = 8, 3
    if exp == "Sad":
        rx = 5
    ry += 7 * open_
    rx -= 1.5 * open_
    ellipse(d, cx, cy, rx, ry, BENI)
    if open_ > 0.15:
        ellipse(d, cx, cy + 0.5, rx - 2.5, ry - 2.5, BENI_DARK)


def face(exp="Neutral", mouth_open=0.0, scale=2):
    img = Image.new("RGB", (320 * SS, 240 * SS), GOFUN)
    d = ImageDraw.Draw(img)
    # Face::draw と同じ順番：口 → 右目 → 左目 → 右眉 → 左眉
    kuchi(d, 160, 182, exp, mouth_open)
    eye(d, 110, 118, False, exp)
    eye(d, 210, 118, True, exp)
    mayu(d, 110, 58, False, exp)
    mayu(d, 210, 58, True, exp)
    return img.resize((320 * scale, 240 * scale), Image.LANCZOS)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    face().save(OUT / "face.png")

    items = [("Neutral", "通常", 0), ("Happy", "をかし（笑顔）", 0), ("Neutral", "話している", 0.7),
             ("Sad", "あはれ（悲しい）", 0), ("Doubt", "はて？（疑問）", 0), ("Sleepy", "ねむし（眠い）", 0)]
    w, h, pad, label = 320, 240, 16, 30
    sheet = Image.new("RGB", (3 * w + 4 * pad, 2 * (h + label) + 3 * pad), (255, 255, 255))
    font = ImageFont.truetype("/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", 18)
    d = ImageDraw.Draw(sheet)
    for i, (exp, name, mo) in enumerate(items):
        x = pad + (i % 3) * (w + pad)
        y = pad + (i // 3) * (h + label + pad)
        sheet.paste(face(exp, mo, scale=1), (x, y))
        d.rectangle([x - 1, y - 1, x + w, y + h], outline=(200, 200, 200))
        d.text((x + w / 2, y + h + 6), name, fill=(60, 60, 60), font=font, anchor="mt")
    sheet.save(OUT / "face_expressions.png")
    print("saved", OUT / "face.png", OUT / "face_expressions.png")


if __name__ == "__main__":
    main()
