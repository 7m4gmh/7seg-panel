#!/usr/bin/env bash
# 受信側(Rockなど)のデコード可否を単体検証
# 使い方: ./tools/recv_rtp_probe.sh [VIDEO_PORT] [AUDIO_PORT]
# 例   : ./tools/recv_rtp_probe.sh 9999 10000
set -euo pipefail

VPORT="${1:-5004}"
APORT="${2:-5006}"

echo "[VIDEO] probing on port ${VPORT} ..."
gst-launch-1.0 -v \
  udpsrc port="${VPORT}" caps="application/x-rtp,media=video,encoding-name=H264,clock-rate=90000,payload=96" \
  ! rtpjitterbuffer latency=80 \
  ! rtph264depay ! h264parse disable-passthrough=true ! avdec_h264 \
  ! fakesink sync=false -e

echo "[AUDIO] probing on port ${APORT} ..."
gst-launch-1.0 -v \
  udpsrc port="${APORT}" caps="application/x-rtp,media=audio,encoding-name=OPUS,clock-rate=48000,payload=97" \
  ! rtpjitterbuffer latency=80 \
  ! rtpopusdepay ! opusdec ! fakesink sync=false -e