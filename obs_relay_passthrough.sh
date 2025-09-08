#!/usr/bin/env bash
set -euo pipefail

# OBS → (localhost) → ffmpeg 中継 → RTP(H.264/Opus) を 7seg-rtp-player へ送出（極力パススルー）
# 使い方:
#   ./obs_relay_passthrough.sh <receiver_ip> [video_port=5004] [audio_port=5006] [ingest_url]
# 例:
#   ./obs_relay_passthrough.sh 192.168.10.107 5004 5006 srt://0.0.0.0:9000?mode=listener&latency=120
#   ./obs_relay_passthrough.sh 192.168.10.107            # 既定: 5004/5006, SRT listener 9000
#
# ポイント:
# - 映像: 可能な限り -c:v copy（H.264想定）。OBS 側が H.265/AV1 等だと rtp_player 側要件と不一致になるため H.264 にしてください。
# - 音声: 既に Opus なら -c:a copy。AAC 等なら libopus でのみ変換（最小限）。
#   ↳ 完全パススルーを狙う場合は OBS 側の音声エンコーダを libopus に設定してください。
#
# OBS 側設定（例・完全パススルーに近づける）:
#   設定 > 出力 > 録画 > 種類: カスタム出力(FFmpeg)
#   コンテナ: matroska（mkv 推奨。SRT で Opus を安全に運べます）
#   ファイルのパスまたはURL: srt://127.0.0.1:9000?mode=caller&latency=120
#   エンコーダ(映像): libx264（H.264）
#   音声エンコーダ: libopus（Opus）
#
# 受信側想定: rtp_player.cpp（H.264 PT=96, OPUS PT=97, 既定ポート 5004/5006）

DEST_IP=${1:-}
VIDEO_PORT=${2:-5004}
AUDIO_PORT=${3:-5006}
INGEST_URL=${4:-"srt://0.0.0.0:9000?mode=listener&latency=120"}

if [[ -z "${DEST_IP}" ]]; then
  echo "Usage: $0 <receiver_ip> [video_port=5004] [audio_port=5006] [ingest_url]" >&2
  exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ffmpeg not found." >&2
  exit 1
fi

# 環境変数で調整可能
VIDEO_PT=${VIDEO_PT:-96}
AUDIO_PT=${AUDIO_PT:-97}
A_BITRATE=${A_BITRATE:-128k}
A_COPY=${A_COPY:-0}   # 1 なら音声も copy（入力が Opus のときに使う）

set -x
if [[ "${A_COPY}" == "1" ]]; then
  ffmpeg \
    -fflags +genpts -flush_packets 1 -max_interleave_delta 0 \
    -i "${INGEST_URL}" \
    -map 0:v:0 -c:v copy -f rtp -payload_type ${VIDEO_PT} "rtp://${DEST_IP}:${VIDEO_PORT}" \
    -map 0:a:0 -c:a copy -f rtp -payload_type ${AUDIO_PT} "rtp://${DEST_IP}:${AUDIO_PORT}"
else
  ffmpeg \
    -fflags +genpts -flush_packets 1 -max_interleave_delta 0 \
    -i "${INGEST_URL}" \
    -map 0:v:0 -c:v copy -f rtp -payload_type ${VIDEO_PT} "rtp://${DEST_IP}:${VIDEO_PORT}" \
    -map 0:a:0 -c:a libopus -b:a "${A_BITRATE}" -ar 48000 -ac 2 -f rtp -payload_type ${AUDIO_PT} "rtp://${DEST_IP}:${AUDIO_PORT}"
fi
