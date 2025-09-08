#!/usr/bin/env bash
set -euo pipefail

# 入力（SRT/RTMP/ローカルファイル等）を H.264/AAC の FLV にし、TCP で 7seg-net-player(fl v) へ送出
# 使い方:
#   ./send_flv_over_tcp.sh <host> [port=5004] [input_url]
# 例:
#   ./send_flv_over_tcp.sh 192.168.10.107 5004 srt://127.0.0.1:9000?mode=caller&latency=120

HOST=${1:-}
PORT=${2:-5004}
INPUT=${3:-/dev/video0}

if [[ -z "${HOST}" ]]; then
  echo "Usage: $0 <host> [port=5004] [input_url]" >&2
  exit 1
fi

FFMPEG=ffmpeg
if [[ -x /usr/local/bin/ffmpeg ]]; then FFMPEG=/usr/local/bin/ffmpeg; fi
if ! command -v "${FFMPEG}" >/dev/null 2>&1; then
  echo "ffmpeg not found" >&2
  exit 1
fi

V_COPY=${V_COPY:-0}    # 1なら映像copy（H.264前提）
A_COPY=${A_COPY:-0}    # 1なら音声copy（AAC前提）
VBPS=${VBPS:-2000k}
ABPS=${ABPS:-128k}
FPS=${FPS:-15}

OUT_URL="tcp://${HOST}:${PORT}?listen=0"

set -x
if [[ "${V_COPY}" == "1" && "${A_COPY}" == "1" ]]; then
  "${FFMPEG}" -re -i "${INPUT}" -map 0:v:0 -c:v copy -map 0:a:0 -c:a copy -f flv "${OUT_URL}"
elif [[ "${V_COPY}" == "1" ]]; then
  "${FFMPEG}" -re -i "${INPUT}" -map 0:v:0 -c:v copy -map 0:a:0 -c:a aac -b:a ${ABPS} -ar 48000 -ac 2 -f flv "${OUT_URL}"
elif [[ "${A_COPY}" == "1" ]]; then
  "${FFMPEG}" -re -i "${INPUT}" -map 0:v:0 -c:v libx264 -preset veryfast -b:v ${VBPS} -maxrate ${VBPS} -bufsize $((2*${VBPS%k} ))k -g $((2*${FPS})) -r ${FPS} \
    -map 0:a:0 -c:a copy -f flv "${OUT_URL}"
else
  "${FFMPEG}" -re -i "${INPUT}" -map 0:v:0 -c:v libx264 -preset veryfast -b:v ${VBPS} -maxrate ${VBPS} -bufsize $((2*${VBPS%k}))k -g $((2*${FPS})) -r ${FPS} \
    -map 0:a:0 -c:a aac -b:a ${ABPS} -ar 48000 -ac 2 -f flv "${OUT_URL}"
fi
