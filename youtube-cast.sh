#!/usr/bin/env bash

# =======================================================

PASS_FILE="./stream_key.conf"

if [ ! -f "$PASS_FILE" ]; then
    echo "エラー: パスワードファイルが見つかりません: $PASS_FILE"
    exit 1
fi

read -r STREAMKEY < "$PASS_FILE"

if [ -z "$STREAMKEY" ]; then
    echo "エラー: パスワードファイルが空か、読み込みに失敗しました。"
    exit 1
fi
VIDEO_DEVICE="/dev/video1"

# フォーカス維持ループ（並行実行）
(
  while true; do
    v4l2-ctl -d ${VIDEO_DEVICE} -c focus_absolute=460
    sleep 600   # 10分ごとに再設定
  done
) &

# =======================================================
# ▼▼▼ 実行部 ▼▼▼
# =======================================================

# 音声モニターソースを自動で取得します
MONITOR="$(pactl list short sources | awk '/monitor/ {print $2; exit}')"
if [[ -z "${MONITOR}" ]]; then
  echo "ERROR: Could not find an audio monitor source." >&2
  exit 1
fi
echo "INFO: Using audio monitor source: ${MONITOR}"

# CPUエンコーダ(libx264)を指定して実行
#  Hardware encoder  h264_rkmpp
# thread_queue_sizeを追加して安定性を向上
#/usr/local/bin/ffmpeg -hide_banner -loglevel info \
# -thread_queue_size 2048 -f v4l2 -input_format mjpeg -framerate 24 -video_size  1280x720 -i "${VIDEO_DEVICE}" \
# -thread_queue_size 2048 -f pulse -i "${MONITOR}" \
# -c:v h264_rkmpp  -b:v 2500k -maxrate 2500k -bufsize 5000k -g 60 \
# -c:a aac -b:a 128k -ar 48000 -ac 2 -r 24 \
# -f flv "rtmp://a.rtmp.youtube.com/live2/${STREAMKEY}"

VIDEO_DEVICE="/dev/video1" # デバイスを video3 に変更

/usr/local/bin/ffmpeg -hide_banner -loglevel info \
 -f v4l2 -input_format yuyv422 -framerate 15 -video_size 800x600 -i "${VIDEO_DEVICE}" \
 -thread_queue_size 2048 -f pulse -i "${MONITOR}" \
 -c:v h264_rkmpp -b:v 1500k -maxrate 1500k -bufsize 3000k -g 30 \
 -c:a aac -b:a 128k -ar 48000 -ac 2 \
 -r 15 \
 -f flv "rtmp://a.rtmp.youtube.com/live2/${STREAMKEY}"

