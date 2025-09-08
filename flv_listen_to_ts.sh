#!/usr/bin/env bash
set -euo pipefail

# FLV(TCP) を受けて、ローカルの MPEG-TS(UDP) にブリッジするリスナー
# 使い方:
#   ./flv_listen_to_ts.sh [tcp_listen_port=5005] [udp_out_port=5004]
# OBS 側URL: tcp://<receiver_ip>:{tcp_listen_port}
# 受信側プレイヤ: ./7seg-net-player ts <config> {udp_out_port}

TCP_PORT=${1:-5005}
UDP_PORT=${2:-5004}

FFMPEG=ffmpeg
if [[ -x /usr/local/bin/ffmpeg ]]; then FFMPEG=/usr/local/bin/ffmpeg; fi
command -v "${FFMPEG}" >/dev/null 2>&1 || { echo "ffmpeg not found" >&2; exit 1; }

# 環境変数で調整
A_COPY=${A_COPY:-1}      # 入力がAACならコピーを既定
A_BR=${A_BR:-128k}
A_SR=${A_SR:-48000}
A_CH=${A_CH:-2}

OUT_URL="udp://127.0.0.1:${UDP_PORT}?pkt_size=1316&overrun_nonfatal=1&fifo_size=5242880"

set -x
if [[ "${A_COPY}" == "1" ]]; then
  "${FFMPEG}" -hide_banner -loglevel info -fflags +genpts -flush_packets 1 -max_interleave_delta 0 \
    -listen 1 -f flv -i "tcp://0.0.0.0:${TCP_PORT}" \
    -map 0:v:0 -c:v copy -bsf:v h264_mp4toannexb \
    -map 0:a:0 -c:a copy \
    -muxdelay 0 -muxpreload 0 -f mpegts "${OUT_URL}"
else
  "${FFMPEG}" -hide_banner -loglevel info -fflags +genpts -flush_packets 1 -max_interleave_delta 0 \
    -listen 1 -f flv -i "tcp://0.0.0.0:${TCP_PORT}" \
    -map 0:v:0 -c:v copy -bsf:v h264_mp4toannexb \
    -map 0:a:0 -c:a aac -b:a "${A_BR}" -ar "${A_SR}" -ac "${A_CH}" \
    -muxdelay 0 -muxpreload 0 -f mpegts "${OUT_URL}"
fi
