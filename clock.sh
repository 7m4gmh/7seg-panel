#!/bin/bash
SIZE="${1:-90}"   # 引数未指定なら 90
MODE="${2:-step}"   
echo "Screen size: $SIZE, Mode: $MODE" >&2
python clock-ffplay-mode.py "$SIZE" "$MODE" | \
    ffplay -f rawvideo -pixel_format rgb24 -video_size "${SIZE}x${SIZE}" -framerate 30 -
