#!/usr/bin/env bash
# 使い方: ./tools/send_rtp_test_gst.sh <RECEIVER_IP> [VIDEO_PORT] [AUDIO_PORT]
# 例   : ./tools/send_rtp_test_gst.sh 192.168.1.50 5004 5006

set -euo pipefail

HOST="${1:-127.0.0.1}"
VPORT="${2:-5004}"
APORT="${3:-5006}"

# 映像: smpte カラーバー → H.264 → RTP → UDP
# 音声: 正弦波 → Opus → RTP → UDP
gst-launch-1.0 -v \
  videotestsrc is-live=true pattern=smpte ! video/x-raw,framerate=20/1 ! videoconvert ! \
  x264enc tune=zerolatency speed-preset=ultrafast bitrate=500 key-int-max=40 ! \
  rtph264pay pt=96 config-interval=1 ! udpsink host="${HOST}" port="${VPORT}" \
  audiotestsrc is-live=true wave=sine freq=440 ! audioconvert ! audioresample ! \
  opusenc bitrate=64000 ! rtpopuspay pt=97 ! udpsink host="${HOST}" port="${APORT}"