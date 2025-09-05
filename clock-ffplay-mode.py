#!/usr/bin/env python3
import sys
import time
import math
import datetime
import numpy as np
from PIL import Image, ImageDraw

# 引数処理
SIZE = 90
MODE = "step"  # デフォルトは1秒ごとのステップ

if len(sys.argv) > 1:
    SIZE = int(sys.argv[1])
if len(sys.argv) > 2:
    MODE = sys.argv[2].lower()
    if MODE not in ("step", "smooth"):
        print("Invalid mode. Use 'step' or 'smooth'.", file=sys.stderr)
        sys.exit(1)

print(f"Clock size: {SIZE}, Mode: {MODE}", file=sys.stderr)

# フレームレート設定
if MODE == "step":
    FPS = 1
else:
    FPS = 30

frame_interval = 1.0 / FPS
next_time = time.time()

def draw_clock(now, size, mode):
    """時計を描画してRGB画像を返す"""
    img = Image.new("RGB", (size, size), (255, 255, 255))  # 白背景
    draw = ImageDraw.Draw(img)

    cx, cy, r = size // 2, size // 2, size // 2 - 4

    # 外枠
    draw.ellipse((cx-r, cy-r, cx+r, cy+r), outline=(0,0,0))

    # 時刻
    h, m, s = now.hour % 12, now.minute, now.second

    # 時針・分針角度
    ha = math.radians((h + m/60) * 30 - 90)
    ma = math.radians(m * 6 - 90)

    # 秒針角度
    if mode == "step":
        sa = math.radians(s * 6 - 90)
    else:  # smooth
        sa = math.radians((s + now.microsecond/1e6) * 6 - 90)

    # 針描画
    draw.line((cx, cy, cx + r*0.5*math.cos(ha), cy + r*0.5*math.sin(ha)), fill=(0,0,0), width=3)
    draw.line((cx, cy, cx + r*0.8*math.cos(ma), cy + r*0.8*math.sin(ma)), fill=(0,0,0), width=2)
    draw.line((cx, cy, cx + r*0.9*math.cos(sa), cy + r*0.9*math.sin(sa)), fill=(255,0,0), width=1)

    return img

# メインループ
while True:
    now = datetime.datetime.now()
    frame = draw_clock(now, SIZE, MODE)

    # numpy 配列に変換してバイト列を書き出す
    arr = np.array(frame, dtype=np.uint8)
    sys.stdout.buffer.write(arr.tobytes())
    sys.stdout.flush()

    # フレーム間隔を正確に保つ
    next_time += frame_interval
    sleep_time = next_time - time.time()
    if sleep_time > 0:
        time.sleep(sleep_time)
    else:
        next_time = time.time()
        