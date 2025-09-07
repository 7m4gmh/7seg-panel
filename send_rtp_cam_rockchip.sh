#!/usr/bin/env bash
set -euo pipefail

# Linux (ROCK5B+/RK3588) 用: カメラ+マイクからRTP送出（H.264 pt=96 / Opus pt=97）
# Usage: ./send_rtp_cam_rockchip.sh HOST [DEVICE] [VPORT] [APORT] [WIDTHxHEIGHT] [FPS] [VBPS_K]
#   HOST           : 受信先IP（例: 192.168.10.107）
#   DEVICE         : V4L2ビデオデバイス（デフォルト /dev/video0）
#   VPORT/APORT    : RTPポート（デフォルト 9999 / 10000）
#   WIDTHxHEIGHT   : 出力解像度（デフォルト 320x240）
#   FPS            : フレームレート（デフォルト 15）
#   VBPS_K         : ビデオビットレート[kbps]（デフォルト 300）

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 HOST [DEVICE] [VPORT] [APORT] [WIDTHxHEIGHT] [FPS] [VBPS_K]" >&2
  exit 1
fi

HOST="$1"
DEVICE="${2:-/dev/video0}"
VPORT="${3:-9999}"
APORT="${4:-10000}"
RES="${5:-320x240}"
FPS="${6:-15}"
VBPS_K="${7:-300}"

WIDTH="${RES%x*}"
HEIGHT="${RES#*x}"
GOP=$(( FPS * 2 ))
VBPS_BITS=$(( VBPS_K * 1000 ))
ABPS_BITS=96000

echo "RTP sending from Linux camera/mic -> ${HOST} (video:${VPORT} pt=96, audio:${APORT} pt=97)"
echo "Video: device=${DEVICE}, ${WIDTH}x${HEIGHT}@${FPS}fps, bitrate=${VBPS_K}kbps (GOP=${GOP})"

# エンコーダ自動判定（mpph264enc → v4l2h264enc → x264enc）
if gst-inspect-1.0 mpph264enc >/dev/null 2>&1; then
  VENC="mpph264enc rc-mode=cbr bitrate=${VBPS_BITS} gop=${GOP}"
elif gst-inspect-1.0 v4l2h264enc >/dev/null 2>&1; then
  # v4l2h264enc は NV12 を好む環境が多いため、前段でNV12へ変換
  VENC="v4l2h264enc"
else
  # ソフトウェアフォールバック
  VENC="x264enc tune=zerolatency speed-preset=veryfast bitrate=${VBPS_K} key-int-max=${GOP} bframes=0"
fi

# 音声ソース自動判定（Pulse → ALSA → autoaudiosrc）
if command -v pactl >/dev/null 2>&1 && pactl list short sources | grep -q monitor; then
  ASRC="pulsesrc device=$(pactl list short sources | awk '/monitor/ {print $2; exit}')"
elif gst-inspect-1.0 alsasrc >/dev/null 2>&1; then
  ASRC="alsasrc device=default"
else
  ASRC="autoaudiosrc"
fi

gst-launch-1.0 -e -v \
  v4l2src device="${DEVICE}" do-timestamp=true ! queue \
    ! videoconvert ! videoscale ! videorate \
    ! video/x-raw,width=${WIDTH},height=${HEIGHT},framerate=${FPS}/1 \
    ! videoconvert ! video/x-raw,format=NV12 \
    ! ${VENC} \
    ! h264parse config-interval=1 \
    ! rtph264pay pt=96 config-interval=1 \
    ! udpsink host="${HOST}" port=${VPORT} \
  ${ASRC} ! queue \
    ! audioconvert ! audioresample \
    ! audio/x-raw,rate=48000,channels=1 \
    ! opusenc inband-fec=true bitrate=${ABPS_BITS} frame-size=20 \
    ! rtpopuspay pt=97 \
    ! udpsink host="${HOST}" port=${APORT}

exit $?
