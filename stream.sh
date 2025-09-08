#!/usr/bin/env bash
set -euo pipefail

# 7seg streaming helper
# 使い方:
#   ./stream.sh recv flv|ts [config=24x4] [port=5004]
#   ./stream.sh send flv <host> [port=5004] [input_url]
#   ./stream.sh send ts  <host> [port=5004] [ingest_url]
#   ./stream.sh examples
#
# 備考:
#  - send flv: ffmpeg で FLV(H.264/AAC) を TCP 送出（OBS→SRT不要）。
#              入力がH.264/AACなら V_COPY=1 A_COPY=1 で再エンコード回避。
#  - send ts : SRT入力を受けて MPEG-TS(UDP) に中継（obs_relay_ts.sh を呼ぶ）。
#              入力がAACなら A_COPY=1 で音声コピー可。

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
BIN_NET_PLAYER="${SCRIPT_DIR}/7seg-net-player"
SEND_FLV_SH="${SCRIPT_DIR}/send_flv_over_tcp.sh"
RELAY_TS_SH="${SCRIPT_DIR}/obs_relay_ts.sh"

if [[ "${DEBUG:-0}" == "1" ]]; then set -x; fi

usage() {
  cat <<USAGE
Usage:
  ${0##*/} recv flv|ts [config=24x4] [port=5004]
  ${0##*/} send flv <host> [port=5004] [input_url]
  ${0##*/} send ts  <host> [port=5004] [ingest_url]
  ${0##*/} examples

Examples:
  # 受信 (FLV/TCP)
  ${0##*/} recv flv 16x12 5004

  # 受信 (TS/UDP)
  ${0##*/} recv ts 24x4 5004

  # 送信 (FLV/TCP) 入力はテスト映像
  ${0##*/} send flv 192.168.10.107 5004 "-re -f lavfi -i testsrc=size=640x360:rate=15 -f lavfi -i sine=frequency=440:sample_rate=48000"

  # 送信 (FLV/TCP) OBS→TCP(listener)
  ${0##*/} send flv 192.168.10.107 5004 "tcp://0.0.0.0:9000?listen=1"

  # 送信 (TS/UDP) SRT(listener)→TS/UDP
  ${0##*/} send ts  192.168.10.107 5004 "srt://0.0.0.0:9000?mode=listener&latency=120"
USAGE
}

cmd=${1:-}
case "${cmd}" in
  recv)
    mode=${2:-flv}
    cfg=${3:-24x4}
    port=${4:-5004}
    if [[ ! -x "${BIN_NET_PLAYER}" ]]; then
      echo "ERROR: ${BIN_NET_PLAYER} not found or not executable. Run 'make net' first." >&2
      exit 1
    fi
    if [[ "${mode}" != "flv" && "${mode}" != "ts" ]]; then
      echo "ERROR: mode must be 'flv' or 'ts'" >&2
      exit 1
    fi
    exec "${BIN_NET_PLAYER}" "${mode}" "${cfg}" "${port}"
    ;;

  send)
    sub=${2:-}
    host=${3:-}
    port=${4:-5004}
    case "${sub}" in
      flv)
        input=${5:-"-re -f lavfi -i testsrc=size=640x360:rate=15 -f lavfi -i sine=frequency=440:sample_rate=48000"}
        if [[ -z "${host}" ]]; then echo "ERROR: host required" >&2; usage; exit 1; fi
        if [[ ! -f "${SEND_FLV_SH}" ]]; then echo "ERROR: ${SEND_FLV_SH} not found" >&2; exit 1; fi
        # input が URL風ならそのまま、そうでなければ ffmpeg引数として解釈されるように前後を整える
        if [[ "${input}" =~ ^(tcp|srt|rtmp|http|https|file):// ]]; then
          exec "${SEND_FLV_SH}" "${host}" "${port}" "${input}"
        else
          # 引数が複数のFFmpegオプション想定の場合は eval で展開する
          # シェル安全性のため、最低限の案内のみ。複雑な場合は直接 send_flv_over_tcp.sh を使用推奨。
          FFMPEG=${FFMPEG:-ffmpeg}
          OUT_URL="tcp://${host}:${port}?listen=0"
          echo "INFO: Running inline ffmpeg with custom input options -> ${OUT_URL}"
          # shellcheck disable=SC2086
          exec ${FFMPEG} ${input} -c:v libx264 -preset veryfast -g 30 -tune zerolatency -c:a aac -b:a 128k -ar 48000 -ac 2 -f flv "${OUT_URL}"
        fi
        ;;
      ts)
        ingest=${5:-"srt://0.0.0.0:9000?mode=listener&latency=120"}
        if [[ -z "${host}" ]]; then echo "ERROR: host required" >&2; usage; exit 1; fi
        if [[ ! -f "${RELAY_TS_SH}" ]]; then echo "ERROR: ${RELAY_TS_SH} not found" >&2; exit 1; fi
        exec "${RELAY_TS_SH}" "${host}" "${port}" "${ingest}"
        ;;
      *)
        echo "ERROR: unknown send mode. Use 'flv' or 'ts'" >&2; usage; exit 1;
        ;;
    esac
    ;;

  examples)
    usage
    ;;

  help|-h|--help|*)
    usage
    ;;
esac
