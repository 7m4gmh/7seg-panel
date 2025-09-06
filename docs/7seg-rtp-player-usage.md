# 7seg-rtp-player の使い方

## 1. ビルド（ワンライナー）
既存の Makefile を触らず試験する場合は、以下のコマンドで `7seg-rtp-player` をビルドできます。

```bash
g++ -std=c++17 -O2 \
  -Iinclude -Isrc \
  src/rtp_player.cpp src/common.cpp src/led.cpp src/video.cpp src/audio.cpp src/playback.cpp \
  -o 7seg-rtp-player \
  $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0 opencv4 sdl2)
```

- 依存が足りない場合（例: `pkg-config` エラー）は `libgstreamer1.0-dev gstreamer1.0-plugins-base libgstreamer-plugins-base1.0-dev libopencv-dev libsdl2-dev` 等を導入してください。

## 2. 起動
```bash
# 例: config.json 内の 16x16_expanded を使い、Video:5004, Audio:5006 を受信
./7seg-rtp-player 16x16_expanded 5004 5006
```

## 3. 送信テスト
別マシン、または同一マシンから以下を実行して映像/音声を送信してください。

### テストパターン（カラーバー + 440Hz）
```bash
chmod +x tools/send_rtp_test_gst.sh
./tools/send_rtp_test_gst.sh <RECEIVER_IP> 5004 5006
```

### 動画ファイル送出
```bash
chmod +x tools/send_rtp_from_file_gst.sh
./tools/send_rtp_from_file_gst.sh <RECEIVER_IP> ./video.mp4 5004 5006
```

## 4. チューニング
- `rtpjitterbuffer latency` を 30–120ms で調整（Wi‑Fi では大きめ）
- `x264enc` の `bitrate` と `key-int-max`（キーフレーム間隔）を環境に合わせて調整
- LED 側の画素に合わせて、受信後のリサイズや減色は `video_thread` 内で既存処理を流用