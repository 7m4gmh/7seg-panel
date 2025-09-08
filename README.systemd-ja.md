# 7seg-net-player を systemd サービスとして動かす手順

本書は 7seg-net-player（FLV/TCP または TS/UDP の受信再生）を systemd サービスとして常時運用するためのセットアップ手順です。

## 前提
- 本リポジトリを /home/radxa/7seg-panel に配置
- バイナリをビルド済み
  - `make net`
- 受信モード:
  - FLV/TCP: `./7seg-net-player flv <config> <port>`
  - TS/UDP : `./7seg-net-player ts  <config> <port>`
- デフォルト動作は EOS/ERROR で即終了（EXIT_ON_EOF=1 相当）

## 導入手順（root 権限）
1. ユニットと環境ファイルを配置
```bash
sudo install -m 644 systemd/7seg-net-player.service /etc/systemd/system/7seg-net-player.service
sudo install -m 644 systemd/7seg-net-player.env /etc/default/7seg-net-player
```

2. 環境ファイルを編集（モード/解像度/ポート等）
```bash
sudo editor /etc/default/7seg-net-player
# 例:
# WORKDIR=/home/radxa/7seg-panel
# MODE=flv
# CONFIG=16x12
# PORT=5004
# EXIT_ON_EOF=1
```

3. 起動・有効化
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now 7seg-net-player.service
```

4. 状態/ログ
```bash
systemctl status 7seg-net-player.service
journalctl -u 7seg-net-player -f
```

## OBS/送信側の設定（FLV/TCP）
- OBS: 録画 → カスタム出力(FFmpeg)
  - コンテナ: flv
  - 出力URL: `tcp://<受信機IP>:<PORT>`
  - 映像: H.264 / 音声: AAC

## 運用メモ
- 本体は EOS/ERROR で即終了 → systemd が自動再起動し、常に待ち受けに戻ります。
- 終了させたくない場合は `/etc/default/7seg-net-player` の `EXIT_ON_EOF=0` に設定してください。
- `User`/`Group` は環境に合わせて `/etc/systemd/system/7seg-net-player.service` を編集してください。
- TS/UDP 運用に切替える場合は `MODE=ts` とし、送信側は MPEG-TS(UDP) を指定してください。

## アンインストール
```bash
sudo systemctl disable --now 7seg-net-player.service
sudo rm -f /etc/systemd/system/7seg-net-player.service
sudo rm -f /etc/default/7seg-net-player
sudo systemctl daemon-reload
```

---
補足: リポジトリ内の `stream.sh` でも同等の起動が可能です（常駐運用には systemd を推奨）。