#!/usr/bin/env bash
set -euo pipefail

# OBS → (localhost) → ffmpeg 中継 → RTP(H.264/Opus) を 7seg-rtp-player へ送出
# 使い方:
#   ./obs_relay_ffmpeg.sh <receiver_ip> [video_port=5004] [audio_port=5006] [ingest_url]
# 例:
#   ./obs_relay_ffmpeg.sh 192.168.10.107 5004 5006 srt://0.0.0.0:9000?mode=listener&latency=120
#   ./obs_relay_ffmpeg.sh 192.168.10.107            # 既定: 5004/5006, SRT listener 9000
#
# OBS 側設定（推奨）:
#   設定 > 出力 > 録画 > 種類: カスタム出力(FFmpeg)
#   コンテナ: mpegts（H.264 + AAC 推奨）
#   ファイルのパスまたはURL: srt://127.0.0.1:9000?mode=caller&latency=120
#   エンコーダ: libx264（配信と同等で可）
#   音声エンコーダ: aac
#
# このスクリプトはローカルで受けた映像/音声を H.264 + Opus に整えて RTP を二系統で送出します。
# 受信側は rtp_player.cpp（H.264 PT=96, OPUS PT=97, 既定ポート 5004/5006）を想定。

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

# エンコード設定（必要に応じて環境変数で変更）
V_BITRATE=${V_BITRATE:-2000k}
GOP=${GOP:-30}
PRESET=${PRESET:-veryfast}
A_BITRATE=${A_BITRATE:-128k}

# 備考: libopus が無い場合は brew/apt 等で ffmpeg を libopus 有効で入れ直してください。

set -x
ffmpeg \
  -fflags +genpts -flush_packets 1 -max_interleave_delta 0 \
  -i "${INGEST_URL}" \
  -map 0:v:0 -c:v libx264 -preset "${PRESET}" -tune zerolatency -g "${GOP}" -keyint_min "${GOP}" -bf 0 -sc_threshold 0 -pix_fmt yuv420p -profile:v main -maxrate "${V_BITRATE}" -bufsize "${V_BITRATE}" -f rtp -payload_type 96 "rtp://${DEST_IP}:${VIDEO_PORT}" \
  -map 0:a:0 -c:a libopus -b:a "${A_BITRATE}" -ar 48000 -ac 2 -f rtp -payload_type 97 "rtp://${DEST_IP}:${AUDIO_PORT}"
