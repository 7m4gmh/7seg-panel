#!/usr/bin/env bash
set -euo pipefail

# OBS/SRT → ffmpeg 中継 → MPEG-TS(H.264/AAC) over UDP → 7seg-net-player(ts)
# 使い方:
#   ./obs_relay_ts.sh <receiver_ip> [port=5004] [ingest_url]
# 例:
#   ./obs_relay_ts.sh 192.168.10.107 5004 srt://0.0.0.0:9000?mode=listener&latency=120
#   A_COPY=1 ./obs_relay_ts.sh 192.168.10.107      # 入力がAACなら音声copy（デフォはAAC変換）
#
# 受信側:
#   ./7seg-net-player ts 24x4 5004

DEST_IP=${1:-}
PORT=${2:-5004}
INGEST_URL=${3:-"srt://0.0.0.0:9000?mode=listener&latency=120"}

if [[ -z "${DEST_IP}" ]]; then
  echo "Usage: $0 <receiver_ip> [port=5004] [ingest_url]" >&2
  exit 1
fi

# ffmpeg の場所を推定
FFMPEG=ffmpeg
if [[ -x /usr/local/bin/ffmpeg ]]; then FFMPEG=/usr/local/bin/ffmpeg; fi
if ! command -v "${FFMPEG}" >/dev/null 2>&1; then
  echo "ffmpeg not found" >&2
  exit 1
fi

# 環境変数
A_COPY=${A_COPY:-0}         # 1 ならAACをcopy（入力がAACの時のみ推奨）
A_BR=${A_BR:-128k}          # 変換時のAACビットレート
A_SR=${A_SR:-48000}
A_CH=${A_CH:-2}

# 送信URL（UDP/TSoUDPの定番パケットサイズ 1316 を使用）
UDP_URL="udp://${DEST_IP}:${PORT}?pkt_size=1316&overrun_nonfatal=1&fifo_size=5242880"

set -x
if [[ "${A_COPY}" == "1" ]]; then
  "${FFMPEG}" -hide_banner -loglevel info \
    -fflags +genpts -flush_packets 1 -max_interleave_delta 0 \
    -i "${INGEST_URL}" \
    -map 0:v:0 -c:v copy -bsf:v h264_mp4toannexb \
    -map 0:a:0 -c:a copy \
    -muxdelay 0 -muxpreload 0 \
    -f mpegts "${UDP_URL}"
else
  "${FFMPEG}" -hide_banner -loglevel info \
    -fflags +genpts -flush_packets 1 -max_interleave_delta 0 \
    -i "${INGEST_URL}" \
    -map 0:v:0 -c:v copy -bsf:v h264_mp4toannexb \
    -map 0:a:0 -c:a aac -b:a "${A_BR}" -ar "${A_SR}" -ac "${A_CH}" \
    -muxdelay 0 -muxpreload 0 \
    -f mpegts "${UDP_URL}"
fi
