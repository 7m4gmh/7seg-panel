#!/usr/bin/env bash
set -euo pipefail

# Usage: ./send_rtp_test_gst.sh [INPUT] [HOST] [VPORT] [APORT]
# Defaults: INPUT=../led/mtknsmb2.mp4, HOST=192.168.10.107, VPORT=9999, APORT=10000

INPUT=${1:-"../led/mtknsmb2.mp4"}
HOST=${2:-"192.168.10.107"}
VPORT=${3:-9999}
APORT=${4:-10000}

echo "Sending RTP to $HOST (video:$VPORT pt=96 / audio:$APORT pt=97)"

gst-launch-1.0 -e -v \
  filesrc location="$INPUT" ! decodebin name=d \
  \
  d. ! queue ! videoconvert ! videoscale ! videorate \
     ! video/x-raw,width=320,height=240,framerate=15/1 \
     ! x264enc tune=zerolatency speed-preset=veryfast bitrate=300 key-int-max=30 bframes=0 \
     ! h264parse config-interval=1 \
     ! rtph264pay pt=96 config-interval=1 \
     ! udpsink host="$HOST" port=$VPORT \
  \
  d. ! queue ! audioconvert ! audioresample \
     ! audio/x-raw,rate=48000,channels=2 \
     ! opusenc inband-fec=true bitrate=96000 frame-size=20 \
     ! rtpopuspay pt=97 \
     ! udpsink host="$HOST" port=$APORT