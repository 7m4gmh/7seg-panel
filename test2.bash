
STREAMKEY="auk9-d9vb-cp3w-x2rz-by5b"

#!/usr/bin/env bash

# =======================================================
# ▼▼▼ 設定する項目 ▼▼▼
# =======================================================


# 2. カメラのデバイスパス
VIDEO_DEVICE="/dev/video1"

# =======================================================
# ▼▼▼ 実行部 ▼▼▼
# =======================================================

# 3. 音声モニターソースを自動で取得します
MONITOR="$(pactl list short sources | awk '/monitor/ {print $2; exit}')"
if [[ -z "${MONITOR}" ]]; then
  echo "ERROR: Could not find an audio monitor source." >&2
  exit 1
fi
echo "INFO: Using audio monitor source: ${MONITOR}"

# 4. 私たちがビルドした正しいFFmpegをフルパスで指定して実行
/usr/local/bin/ffmpeg -hide_banner -loglevel info \
 -f v4l2 -input_format mjpeg -framerate 30 -video_size 1280x720 -i "${VIDEO_DEVICE}" \
 -f pulse -i "${MONITOR}" \
 -vf "format=yuv420p" \
 -c:v h264_v4l2m2m -b:v 4000k -maxrate 4000k -bufsize 8000k -g 60 \
 -c:a aac -b:a 128k -ar 44100 -ac 2 \
 -f flv "rtmp://a.rtmp.youtube.com/live2/${STREAMKEY}"

