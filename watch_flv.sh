#!/usr/bin/env bash
set -euo pipefail

# 再接続待ちを強化するための外部ウォッチャ
# 使い方: ./watch_flv.sh [config=24x4] [port=5004]

CFG=${1:-24x4}
PORT=${2:-5004}

BIN=./7seg-net-player
if [[ ! -x ${BIN} ]]; then echo "ERROR: ${BIN} not found. Run 'make net'" >&2; exit 1; fi

while true; do
  echo "[watch] starting ${BIN} flv ${CFG} ${PORT}"
  set +e
  ${BIN} flv "${CFG}" "${PORT}"
  code=$?
  set -e
  echo "[watch] exited with code ${code}. restarting in 1s..."
  sleep 1
done
