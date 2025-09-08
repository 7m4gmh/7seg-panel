#!/usr/bin/env bash
set -euo pipefail

# 送信テスト用スクリプト（FFmpeg）
# 使い方:
#   ./test_send.sh [ts|srt] <receiver_ip> [port] [input]
# 例:
#   ./test_send.sh ts 192.168.1.50 5004
#   ./test_send.sh srt 192.168.1.50 6000 /path/to/video.mp4
# 入力未指定時はテストパターンを生成。

# 互換のため事前に空配列を宣言（set -uでも未定義展開を防ぐ）
declare -a VIDEO_SRC=()
declare -a AUDIO_SRC=()
declare -a MAPS=()
declare -a COMMON_ENCODERS=()
declare -a MUX_OPTS=()

MODE=${1:-ts}
DEST_IP=${2:-}
PORT=${3:-}
INPUT=${4:-}

if [[ -z "${DEST_IP}" ]]; then
  echo "Usage: $0 [ts|srt] <receiver_ip> [port] [input]" >&2
  exit 1
fi

if [[ -z "${PORT}" ]]; then
  if [[ "${MODE}" == "srt" ]]; then PORT=6000; else PORT=5004; fi
fi

# 低遅延寄りのH.264/AAC設定（必要に応じて環境変数で上書き可能）
V_BITRATE=${V_BITRATE:-2000k}
A_BITRATE=${A_BITRATE:-128k}
GOP=${GOP:-30}
PRESET=${PRESET:-veryfast}
SRT_LAT=${SRT_LAT:-150}
SRT_PARMS="mode=caller&latency=${SRT_LAT}&sndbuf=8388608"

if [[ -z "${INPUT}" ]]; then
  # 生成: 320x240@15 のテストパターン + サイン波音
  VIDEO_SRC=( -f lavfi -re -i "testsrc=size=320x240:rate=15" )
  AUDIO_SRC=( -f lavfi -re -i "sine=frequency=440:sample_rate=48000:duration=3600" )
  MAPS=( -map 0:v:0 -map 1:a:0 )
else
  VIDEO_SRC=( -re -i "${INPUT}" )
  AUDIO_SRC=()
  MAPS=()
fi

COMMON_ENCODERS=( \
  -c:v libx264 -preset "${PRESET}" -tune zerolatency -g "${GOP}" -keyint_min "${GOP}" -bf 0 -sc_threshold 0 \
  -pix_fmt yuv420p -profile:v main -maxrate "${V_BITRATE}" -bufsize "${V_BITRATE}" \
  -c:a aac -b:a "${A_BITRATE}" -ar 48000 -ac 2 \
)

if [[ "${MODE}" == "ts" ]]; then
  URL="udp://${DEST_IP}:${PORT}"
  MUX_OPTS=( -f mpegts )
elif [[ "${MODE}" == "srt" ]]; then
  URL="srt://${DEST_IP}:${PORT}?${SRT_PARMS}"
  MUX_OPTS=( -f mpegts )
else
  echo "Unknown mode: ${MODE}" >&2; exit 1
fi

# ffmpeg存在チェック
if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg not found. Please install ffmpeg." >&2
  exit 1
fi

CMD=( ffmpeg )
CMD+=( "${VIDEO_SRC[@]}" )
if [[ ${#AUDIO_SRC[@]} -gt 0 ]]; then CMD+=( "${AUDIO_SRC[@]}" ); fi
if [[ ${#MAPS[@]} -gt 0 ]]; then CMD+=( "${MAPS[@]}" ); fi
CMD+=( "${COMMON_ENCODERS[@]}" )
CMD+=( "${MUX_OPTS[@]}" )
CMD+=( -avoid_negative_ts make_zero -fflags +genpts -flush_packets 1 -max_interleave_delta 0 -y "${URL}" )

set -x
"${CMD[@]}"
