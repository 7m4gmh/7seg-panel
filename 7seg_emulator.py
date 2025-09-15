### 7seg_emulator.py
# 7セグメントLEDのエミュレーター
# tkinterを使用してGUIで表示
#　実行方法: python3 7seg_emulator.py



import sys
import tkinter as tk
import argparse


# 実物サイズ・傾斜・余白を見本図面に合わせて再設計
# 外形: 12.7mm x 19.05mm（10倍スケール: 127x191）
# ビット順: a(0),b(1),c(2),d(3),e(4),f(5),g(6),dp(7)

import math
CANVAS_W = 160
CANVAS_H = 220
SEG_W = 14   # セグメントの太さ（図面比率で調整）
SEG_L = 92   # 横セグメントの長さ
SEG_H = 68   # 縦セグメントの長さ
OFFSET_X = 28
OFFSET_Y = 28
DP_R = 9     # dpの半径
SKEW_DEG = 8
SKEW_RAD = math.radians(SKEW_DEG)

def skew_vertical(x, y, cx, cy):
    # 垂直セグメントのみ傾斜
    dx = x - cx
    dy = y - cy
    nx = dx * math.cos(SKEW_RAD) - dy * math.sin(SKEW_RAD)
    ny = dx * math.sin(SKEW_RAD) + dy * math.cos(SKEW_RAD)
    return (cx + nx, cy + ny)

# セグメント中心線の基準点
cx = OFFSET_X + SEG_L/2
cy = OFFSET_Y + SEG_H





# matplotlib例のロジックを厳密にTkinterへ移植
def rotate_points(points, angle_deg, origin=(0,0)):
    angle = math.radians(angle_deg)
    ox, oy = origin
    rot = []
    for x, y in points:
        qx = ox + math.cos(angle) * (x - ox) - math.sin(angle) * (y - oy)
        qy = oy + math.sin(angle) * (x - ox) + math.cos(angle) * (y - oy)
        rot.append((qx, qy))
    return rot

def horizontal_segment(x, y, scale=1):
    w, h = 4 * scale, 1 * scale
    cut = h / 2
    # 端2点が90度、他4点が135度になる六角形
    coords = [
        (x - w/2 + cut, y - h/2),  # 左上斜め
        (x + w/2 - cut, y - h/2),  # 右上斜め
        (x + w/2,       y),        # 右直角
        (x + w/2 - cut, y + h/2),  # 右下斜め
        (x - w/2 + cut, y + h/2),  # 左下斜め
        (x - w/2,       y),        # 左直角
    ]
    return coords

def vertical_segment(x, y, scale=1, tilt=10):
    # 横セグメント六角形を90±tilt度回転して流用
    base = horizontal_segment(0, 0, scale)
    coords = rotate_points(base, 90 + tilt, origin=(0, 0))
    coords = [(x + px, y + py) for (px, py) in coords]
    return coords

def seg_pts(raw_pts):
    return raw_pts


SCALE = 18
X0 = 60
Y0 = 80

# 六角形の中心座標・角度をセグメントごとに調整



# 実物サイズ比率で配置
CANVAS_W = 200
CANVAS_H = 300
SEG_L = 0.63   # 横セグメント長さ比率 (12.7mm/12.7mm)
SEG_H = 0.42   # 縦セグメント長さ比率 (8mm/19.05mm)
SEG_W = 0.08   # セグメント幅比率 (1mm/12.7mm)
TILT = 10      # degree


# セグメント配置の最大幅・高さ（比率ベース）
SEG_L = 4.0   # 横セグメント長さ
SEG_H = 6.0   # 縦セグメント長さ
SEG_W = 1.0   # セグメント幅
TILT = 10

# 配置範囲（中心からの最大オフセット）
X_RANGE = 3.0  # セグメント中心の最大xオフセット
Y_RANGE = 4.0  # セグメント中心の最大yオフセット

# スケール自動調整
max_scale_w = (CANVAS_W * 0.9) / ((X_RANGE + SEG_L/2) * 2)
max_scale_h = (CANVAS_H * 0.9) / ((Y_RANGE + SEG_W) * 2)
SCALE = min(max_scale_w, max_scale_h)

# 中心座標
X0 = CANVAS_W // 2
Y0 = CANVAS_H // 2

def seg_x(x):
    return X0 + x * SCALE
def seg_y(y):
    return Y0 - y * SCALE

# dセグメントの中心座標
d_cx = seg_x(0)
d_cy = seg_y(-Y_RANGE)
# DPの中心座標: Xは右端から線幅/2+余白, Yはdセグメントと同じ
dp_r = SCALE * SEG_W / 2
dp_cx = CANVAS_W - dp_r - 4
dp_cy = d_cy

SEGMENTS = [
    # a: 上横（右に0.5単位ずらす）
    horizontal_segment(seg_x(0.5), seg_y(Y_RANGE), scale=SCALE),
    # b: 右上縦（下に1単位ずらす）
    vertical_segment(seg_x(X_RANGE), seg_y(Y_RANGE-1-1), scale=SCALE, tilt=TILT),
    # c: 右下縦（上に1単位、左に0.5単位ずらす）
    vertical_segment(seg_x(X_RANGE-0.5), seg_y(-Y_RANGE+1+1), scale=SCALE, tilt=TILT),
    # d: 下横（左に0.5単位ずらす）
    horizontal_segment(seg_x(-0.5), seg_y(-Y_RANGE), scale=SCALE),
    # e: 左下縦（上に1単位ずらす）
    vertical_segment(seg_x(-X_RANGE), seg_y(-Y_RANGE+1+1), scale=SCALE, tilt=TILT),
    # f: 左上縦（下に1単位、右に0.5単位ずらす）
    vertical_segment(seg_x(-X_RANGE+0.5), seg_y(Y_RANGE-1-1), scale=SCALE, tilt=TILT),
    # g: 中央横
    horizontal_segment(seg_x(0), seg_y(0), scale=SCALE),
    # dp: dセグメントと同じ高さ、右端から左に1単位ずらす
    [
        (dp_cx - SCALE * 1.0, dp_cy),
        dp_r
    ]
]

# 各数字に対応するセグメントのON/OFF（a,b,c,d,e,f,g,dp）
DIGIT_TO_SEG = [
    [1,1,1,1,1,1,0,0], # 0
    [0,1,1,0,0,0,0,0], # 1
    [1,1,0,1,1,0,1,0], # 2
    [1,1,1,1,0,0,1,0], # 3
    [0,1,1,0,0,1,1,0], # 4
    [1,0,1,1,0,1,1,0], # 5
    [1,0,1,1,1,1,1,0], # 6
    [1,1,1,0,0,0,0,0], # 7
    [1,1,1,1,1,1,1,0], # 8
    [1,1,1,1,0,1,1,0], # 9
]



def draw_segments(canvas, seg_bits):
    for i, seg in enumerate(SEGMENTS):
        if i == 7:  # dp
            color = "red" if (seg_bits >> 7) & 1 else "gray"
            (cx, cy), r = seg
            canvas.create_oval(cx-r, cy-r, cx+r, cy+r, fill=color, outline="black", width=2)
        else:
            color = "red" if (seg_bits >> i) & 1 else "gray"
            canvas.create_polygon(seg, fill=color, outline="black", width=2)


def draw_digit(canvas, digit):
    seg_bits = 0
    if 0 <= digit <= 9:
        for i, on in enumerate(DIGIT_TO_SEG[digit]):
            if on:
                seg_bits |= (1 << i)
    draw_segments(canvas, seg_bits)




def main():
    parser = argparse.ArgumentParser(description="7セグメントLEDエミュレータ")
    parser.add_argument("value", nargs="?", default="8", help="表示する値 (digitモード:0-9, segモード:ビット値)")
    parser.add_argument("--mode", choices=["digit", "seg"], default="digit", help="表示モード: digit(数字), seg(ビット指定)")
    args = parser.parse_args()

    root = tk.Tk()
    root.title("7-Segment LED Emulator")
    canvas = tk.Canvas(root, width=CANVAS_W, height=CANVAS_H, bg="black", highlightthickness=0)
    canvas.pack()

    if args.mode == "digit":
        try:
            digit = int(args.value)
            if 0 <= digit <= 9:
                draw_digit(canvas, digit)
            else:
                draw_digit(canvas, 8)
        except ValueError:
            draw_digit(canvas, 8)
    else:  # segモード
        try:
            if args.value.startswith("0x") or args.value.startswith("0X"):
                seg_bits = int(args.value, 16)
            elif args.value.startswith("0b") or args.value.startswith("0B"):
                seg_bits = int(args.value, 2)
            else:
                seg_bits = int(args.value)
            seg_bits &= 0xFF  # 8bit (a-g,dp)
        except Exception:
            seg_bits = 0
        draw_segments(canvas, seg_bits)

    root.mainloop()

if __name__ == "__main__":
    main()