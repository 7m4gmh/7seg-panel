#!/usr/bin/env bash

# =======================================================
# ▼▼▼ 設定する項目 ▼▼▼
# =======================================================

# 1. YouTubeのストリームキーをここに設定してください
STREAMKEY="auk9-d9vb-cp3w-x2rz-by5b"

# 2. カメラのデバイスパス
VIDEO_DEVICE="/dev/video1"

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
# thread_queue_sizeを追加して安定性を向上
/usr/local/bin/ffmpeg -hide_banner -loglevel info \
 -thread_queue_size 1024 -f v4l2 -input_format mjpeg -framerate 30 -video_size 1280x720 -i "${VIDEO_DEVICE}" \
 -thread_queue_size 1024 -f pulse -i "${MONITOR}" \
 -c:v libx264 -preset veryfast -b:v 4000k -maxrate 4000k -bufsize 8000k -g 60 \
 -c:a aac -b:a 128k -ar 44100 -ac 2 \
 -f flv "rtmp://a.rtmp.youtube.com/live2/${STREAMKEY}"

