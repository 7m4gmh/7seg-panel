# 7-Segment LED Panel Video Player

A series of tools for playing videos on a custom display panel composed of I2C-connected 7-segment LED modules.

i2cで駆動する自作の7セグメントLEDパネルで、動画を再生するプロジェクト。

---

**Languages:**

[**English**](README.en.md) | [**日本語**](README.ja.md) | [** 繁體中文**](README.zh-TW.md)

---

[![デモ動画](./docs/7seg-output_hq.gif)](https://www.instagram.com/reel/DOIo3QTEZs0/?utm_source=ig_web_button_share_sheet)

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
make streamer   # 7seg-http-streamer

# Everything
make all

# Cleanup
make clean
```