#!/usr/bin/env bash
set -euo pipefail

# macOSのカメラ/マイクからRTP送出（H.264 pt=96 → VPORT, Opus pt=97 → APORT）
# Usage: ./send_rtp_cam_gst.sh HOST [VIDEO_IDX] [AUDIO_IDX] [VPORT] [APORT]
#   HOST      : 受信先IP（例: 192.168.10.107）
#   VIDEO_IDX : avfvideosrc の device-index（デフォルト0）
#   AUDIO_IDX : avfaudiosrc の device-index（デフォルト0）
#   VPORT     : 映像RTPポート（デフォルト9999）
#   APORT     : 音声RTPポート（デフォルト10000）

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 HOST [VIDEO_IDX] [AUDIO_IDX] [VPORT] [APORT]" >&2
  exit 1
fi

HOST="$1"
VIDEO_IDX="${2:-0}"
AUDIO_IDX="${3:-0}"
VPORT="${4:-9999}"
APORT="${5:-10000}"

# 推奨ビットレートなど
VBPS_K=300        # x264enc 用（kbps）
VBPS_BITS=300000  # vtenc_h264 用（bps）
ABPS_BITS=96000   # Opus 96kbps
FPS=15
WIDTH=320
HEIGHT=240
GOP=30

echo "RTP sending from macOS camera/mic -> $HOST (video:$VPORT pt=96, audio:$APORT pt=97)"
echo "Video device-index=$VIDEO_IDX, Audio device-index=$AUDIO_IDX"

# エンコーダ自動判定（VideoToolbox 優先）
if gst-inspect-1.0 vtenc_h264 >/dev/null 2>&1; then
  VENC="vtenc_h264 realtime=true allow-frame-reordering=false bitrate=${VBPS_BITS} max-keyframe-interval=${GOP}"
else
  VENC="x264enc tune=zerolatency speed-preset=veryfast bitrate=${VBPS_K} key-int-max=${GOP} bframes=0"
fi

gst-launch-1.0 -e -v \
  avfvideosrc device-index=${VIDEO_IDX} ! queue \
    ! videoconvert ! videoscale ! videorate \
    ! video/x-raw,width=${WIDTH},height=${HEIGHT},framerate=${FPS}/1 \
    ! ${VENC} \
    ! h264parse config-interval=1 \
    ! rtph264pay pt=96 config-interval=1 \
    ! udpsink host="${HOST}" port=${VPORT} \
  avfaudiosrc device-index=${AUDIO_IDX} ! queue \
    ! audioconvert ! audioresample \
    ! audio/x-raw,rate=48000,channels=1 \
    ! opusenc inband-fec=true bitrate=${ABPS_BITS} frame-size=20 \
    ! rtpopuspay pt=97 \
    ! udpsink host="${HOST}" port=${APORT}
