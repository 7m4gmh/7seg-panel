#!/usr/bin/env bash
# 使い方: ./tools/send_rtp_test_gst.sh <RECEIVER_IP> [VIDEO_PORT] [AUDIO_PORT]
# 例   : ./tools/send_rtp_test_gst.sh 192.168.10.107 9999 10000
set -euo pipefail

HOST="${1:-127.0.0.1}"
VPORT="${2:-5004}"
APORT="${3:-5006}"

pick_venc() {
  if gst-inspect-1.0 x264enc >/dev/null 2>&1; then
    echo 'x264enc tune=zerolatency speed-preset=ultrafast bitrate=800 key-int-max=40 profile=baseline byte-stream=true'
    return
  fi
  if gst-inspect-1.0 vtenc_h264 >/dev/null 2>&1; then
    echo 'vtenc_h264 realtime=true allow-frame-reordering=false max-keyframe-interval=40 bitrate=800000'
    return
  fi
  if gst-inspect-1.0 avenc_h264 >/dev/null 2>&1; then
    echo 'avenc_h264 bitrate=800000 g=40'
    return
  fi
  if gst-inspect-1.0 openh264enc >/dev/null 2>&1; then
    echo 'openh264enc bitrate=800000 gop-size=40'
    return
  fi
  echo ""
}

VENC="$(pick_venc)"
if [ -z "${VENC}" ]; then
  echo "No H.264 encoder found (x264enc/vtenc_h264/avenc_h264/openh264enc)." >&2
  echo "Install one of: gst-plugins-ugly (x264enc), gst-plugins-bad (vtenc_h264, openh264enc with openh264), gst-libav (avenc_h264)." >&2
  exit 1
fi

gst-launch-1.0 -v \
  videotestsrc is-live=true pattern=smpte ! video/x-raw,format=I420,framerate=20/1 ! \
  ${VENC} ! h264parse ! rtph264pay pt=96 config-interval=1 ! udpsink host="${HOST}" port="${VPORT}" \
  audiotestsrc is-live=true wave=sine freq=440 ! audioconvert ! audioresample ! \
  opusenc bitrate=64000 ! rtpopuspay pt=97 ! udpsink host="${HOST}" port="${APORT}"

