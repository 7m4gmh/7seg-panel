# 7-Segment LED Panel Video Player

A series of tools for playing videos on a custom display panel composed of I2C-connected 7-segment LED modules.

i2cで駆動する自作の7セグメントLEDパネルで、動画を再生するプロジェクト。

---

Note (platform): Developed and tested primarily on Radxa ROCK 5B. It may also work on Raspberry Pi 4/5 if I2C is enabled and dependencies are installed (bus numbers/device names may differ).

**macOS Support**: The project includes emulator mode for macOS development and testing. Physical LED panels are automatically simulated with accurate segment rendering and audio synchronization.

注記（プラットフォーム）: 本プロジェクトは主に Radxa ROCK 5B 向けに作成・検証しています。I2C を有効化し依存関係を整えれば Raspberry Pi 4/5 でも動作する可能性があります（バス番号やデバイス名が異なる場合があります）。

**macOS対応**: 本プロジェクトはmacOSでの開発・テスト用にエミュレータモードを搭載しています。物理的なLEDパネルを自動的にシミュレートし、正確なセグメント描画と音声同期を実現します。

---

**Languages:**

[**English**](README.en.md) | [**日本語**](README.ja.md) | [**繁體中文**](README.zh-TW.md)

---

[![デモ動画](./docs/7seg-output_hq.gif)](https://www.instagram.com/reel/DOIo3QTEZs0/?utm_source=ig_web_button_share_sheet)


---

## Hardware (KiCad)

- PCB projects live under `hardware/`. See: [hardware/README.md](hardware/README.md)
- Current folders (subject to change):
	- `hardware/7seg-led` — LED panel PCB
	- `hardware/7seg-control` — main controller PCB
	- `hardware/7seg-hat` — Raspberry Pi HAT
	- `hardware/7seg-power` — power board
	- Shared libs: `hardware/lib`
	- Legacy/migration folders may also exist (e.g., `hardware/led-panel`, `controller`, `rpi-hat`, `power`).

---
## Build

Prerequisites
- C++17-capable compiler (g++)
- make
- OpenCV 4 (dev package)
- SDL2 (dev package)
- GStreamer 1.0 (for RTP/HTTP streaming tools)

How to build
- By default, running `make` without arguments shows help and does not build anything.
- Use the grouped targets below as needed.

Common targets
```bash
# Show available targets (default)
make

# Non-GStreamer players only: udp/file/http
make core

# GStreamer-based tools: http-streamer / rtp-player
make gst

# Individual targets
make rtp        # 7seg-rtp-player
# (http-streamerは削除済み)

# Everything
make all

# Cleanup
make clean
```

### macOS Emulator Support
The project includes macOS emulator support for development and testing without physical hardware:
- Automatically detects macOS and uses emulator mode
- Physical LED panel simulation with accurate segment shapes
- Optimized performance with pre-cached layouts
- Audio synchronization for smooth playback

**macOSでのビルド:**
```bash
# OpenCVのインストール
brew install opencv

# ビルド
make core

# エミュレータでのテスト
./7seg-file-player test.mp4 emulator-24x4
```

Tip
- For streaming, we recommend the net player with OBS via FLV/TCP:
	- [README.en.md](README.en.md) → "Send directly from OBS (FLV/TCP)"
	- [README.ja.md](README.ja.md) → 「OBSから直接送信（FLV/TCP）」
	- [README.zh-TW.md](README.zh-TW.md) → 「從 OBS 直接傳送（FLV/TCP）」
- Alternatively, the RTP player is available. See usage and sender examples:
	- [README.en.md](README.en.md) → "RTP Player (Recommended)"
	- [README.ja.md](README.ja.md) → 「RTPプレイヤー（推奨）」
	- [README.zh-TW.md](README.zh-TW.md) → 「RTP 播放器（建議）」
- macOS camera sender script included: `send_rtp_cam_gst.sh`
 - For browser-based control and uploads, use the HTTP player/streamer. See:
	 - [README.en.md](README.en.md) → "HTTP Player"
	 - [README.ja.md](README.ja.md) → 「HTTPプレイヤー」
	 - [README.zh-TW.md](README.zh-TW.md) → 「HTTP 播放器」
 - To play a local video file quickly, use the File Player. See:
	 - [README.en.md](README.en.md) → "File Player"
	 - [README.ja.md](README.ja.md) → 「ファイルプレイヤー」
	 - [README.zh-TW.md](README.zh-TW.md) → 「檔案播放器」

- **macOS Emulator**: For testing and development on macOS without physical hardware. See:
	 - [README.emulator.md](README.emulator.md) → "7-Segment LED Panel Emulator"
	 - 物理的なLEDパネルの正確なシミュレーション
	 - 動画ファイルからの再生と音声同期

- Tetris on the LED panel (Python):
	- [README.tetris.en.md](README.tetris.en.md)
	- [README.tetris.md](README.tetris.md)（日本語）
	- [README.tetris.zh-TW.md](README.tetris.zh-TW.md)



