#!/usr/bin/env bash
set -euo pipefail

# 7seg-net-player 受信テスト用ラッパー
# 使い方:
#   ./test_recv.sh [ts|srt] [config_name] [port]
# 例:
#   ./test_recv.sh ts 24x4 5004
#   ./test_recv.sh srt 24x4 6000

MODE=${1:-ts}
CONFIG=${2:-24x4}
PORT=${3:-}

if [[ -z "${PORT}" ]]; then
  if [[ "${MODE}" == "srt" ]]; then PORT=6000; else PORT=5004; fi
fi

echo "[recv] mode=${MODE} config=${CONFIG} port=${PORT}"
echo "[hint] SRTのレイテンシは現状コード固定です。必要なら後で調整版を反映します。"

exec ./7seg-net-player "${MODE}" "${CONFIG}" "${PORT}"
