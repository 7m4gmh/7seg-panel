# 7-Segment LED Panel Player

## Overview

This project is a comprehensive suite of tools designed to play videos on a custom-built large-scale 7-segment LED panel array. It processes video files in real-time, converting them into signals to drive multiple I2C-based LED modules, managed by a TCA9548A I2C multiplexer.

It features a robust error detection and recovery mechanism, allowing for stable, long-term operation by automatically attempting to recover from unstable I2C communication.

Platform note: The project is primarily developed and tested on Radxa ROCK 5B. It may also work on Raspberry Pi 4/5 provided I2C is enabled and dependencies are installed (bus numbers and device nodes may differ).

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [How to Build](#how-to-build)
- [Configuration](#configuration)
- [Usage](#usage)
  - [File Player (7seg-file-player)](#file-player-7seg-file-player)
  - [HTTP Player (7seg-http-player)](#http-player-7seg-http-player)
  - [RTP Player (Recommended) (7seg-rtp-player)](#rtp-player-recommended-7seg-rtp-player)
  - [UDP Player (7seg-udp-player)](#udp-player-7seg-udp-player)
- [Troubleshooting](#troubleshooting)
  - [Tips for RTP reception](#tips-for-rtp-reception)
- [License](#license)

## Features

- **Multiple Players**:
  - `7seg-file-player`: Plays a local video file.
  - `7seg-http-player`: Provides a web UI for video uploads, queue management, and playback control.
  - `7seg-udp-player`: Receives and displays a real-time UDP stream.
- **Flexible Panel Configuration**: Define the physical layout and I2C addresses of your LED modules freely via `config.json`.
- **Advanced I2C Error Recovery**:
  - Automatically detects communication errors and re-initializes the display.
  - Retries the recovery process multiple times if the initial attempt fails.
- **Hardware Issue Analysis**: On exit (`Ctrl+C`), it displays an aggregated report of errors by I2C channel and address, helping to pinpoint problematic hardware (modules, wiring).
- **Remote Control via Web UI** (`7seg-http-player`):
  - Upload video files.
  - Manage the playback queue (add/delete).
  - Stop the currently playing video.
  - Check playback status.

## Prerequisites

- C++17 compatible compiler (`g++`)
- `make`
- **OpenCV 4** (`libopencv-dev`)
- **libSDL2** (`libsdl2-dev`) (for audio playback)
- **GStreamer** (for streaming from stdin)

## How to Build

See [README.md](README.md)

## Configuration

Edit the `config.json` file to define the hardware configuration of your LED panel.

**Example Configuration (`16x16_expanded`):**
```json
{
  "configs": {
    "16x16_expanded": {
      "tca9548a_address": 119,
      "total_width": 16,
      "total_height": 16,
      "module_digits_width": 16,
      "module_digits_height": 4,
      "channel_grids": {
        "0": [
          [ 112 ]
        ],
        "1": [
          [ 112, 113, 114, 115 ]
        ],
        "2": [
          [ 112, 113, 114, 115 ]
        ]
      }
    }
  }
}
```

- `tca9548a_address`: The decimal address of the TCA9548A I2C multiplexer. Set to `-1` if not in use.
- `total_width`, `total_height`: The total width and height of the panel in number of characters.
- `module_digits_width`, `module_digits_height`: The width and height of a single LED module in number of characters.
- `channel_grids`: Defines the I2C addresses of modules connected to each channel of the TCA9548A in a 2D array.

## Usage

### File Player (`7seg-file-player`)

Plays a local video file.

```bash
./7seg-file-player <path_to_video_file> [config_name]
```
Example: `./7seg-file-player ./videos/my_video.mp4 16x16_expanded`

### HTTP Player (`7seg-http-player`)

Starts a web server for browser-based control.

```bash
./7seg-http-player <path_to_default_videos_dir> [config_name]
```
Example: `./7seg-http-player ./default_videos 16x16_expanded`

After starting the server, access the UI by navigating to `http://<your_pi_ip_address>:8080` in your web browser.

### RTP Player (Recommended) (`7seg-rtp-player`)

RTP reception provides stronger jitter handling and is generally more stable on typical networks.

```bash
./7seg-rtp-player [config_name] [video_port] [audio_port]
```
Example: `./7seg-rtp-player 16x12 9999 10000`

After starting the receiver, send H.264 (pt=96) and Opus (pt=97) RTP streams to the same host.

#### Sender (GStreamer: file → RTP)
```bash
gst-launch-1.0 -e -v \
  filesrc location=INPUT.mp4 ! decodebin name=d \
  d. ! queue ! videoconvert ! videoscale ! videorate \
    ! video/x-raw,width=320,height=240,framerate=15/1 \
    ! x264enc tune=zerolatency speed-preset=veryfast bitrate=300 key-int-max=30 bframes=0 \
    ! h264parse config-interval=1 ! rtph264pay pt=96 config-interval=1 ! udpsink host=ROCK_IP port=9999 \
  d. ! queue ! audioconvert ! audioresample \
    ! audio/x-raw,rate=48000,channels=2 \
    ! opusenc inband-fec=true bitrate=96000 frame-size=20 \
    ! rtpopuspay pt=97 ! udpsink host=ROCK_IP port=10000
```

#### Sender (macOS: camera/mic → RTP)
If GStreamer is installed on your Mac, use the bundled script:

```bash
./send_rtp_cam_gst.sh ROCK_IP 0 0 9999 10000
```
- Automatically selects available encoders and audio sources (VideoToolbox preferred, fallback to x264enc; avfaudiosrc → osxaudiosrc → autoaudiosrc).
- Defaults: 320x240 @ 15fps, H.264 ~300kbps; audio Opus 48kHz 96kbps with FEC.

#### Sender (mac/PC: file → RTP)
Use the bundled file sender script:

```bash
./send_rtp_test_gst.sh [INPUT] [HOST] [VPORT] [APORT]
# Example
./send_rtp_test_gst.sh ../led/mtknsmb2.mp4 192.168.10.107 9999 10000
```

### UDP Player (`7seg-udp-player`)

Listens for a UDP stream on a specified port.

```bash
./7seg-udp-player <port_number> [config_name]
```
Example: `./7seg-udp-player 12345 16x16_expanded`

### Send directly from OBS (FLV/TCP)

Send from OBS via FLV over TCP and let the player receive and display it.

1) Start the receiver (FLV/TCP)
```bash
./7seg-net-player flv 16x12 5004
```

2) OBS settings (Recording tab → Custom Output (FFmpeg))
- Container: flv
- Video: H.264 (x264 or similar)
- Audio: AAC
- Output URL: `tcp://<receiver_ip>:5004`

Notes:
- Use “Custom Output (FFmpeg)”, not RTMP.
- On stream end, the receiver exits on EOF by default (with systemd it will auto-restart).
- For service usage, set `MODE=flv` and `PORT=5004` (see `README.systemd-ja.md` for details).

## Troubleshooting

If you experience frequent I2C communication failures, the program will attempt to recover automatically. When you exit the program with `Ctrl+C`, an analysis report is displayed to help identify the source of the errors.

**Example Analysis Report:**
```
--- I2C Error Analysis ---
Channel: 2, Address: 0x70  => 15 errors
Channel: 2, Address: 0x72  => 42 errors
--------------------------
```
This example indicates that the errors are concentrated on **Channel 2** of the TCA9548A. This strongly suggests a physical problem with the wiring for Channel 2 or with the modules connected to it (`0x70`, `0x72`).

### Tips for RTP reception
- The examples use 9999 for video and 10000 for audio; adjust ports to your environment.
- For stable video, start with 320x240 @ 15fps, H.264 (keyframe=30, zerolatency, bframes=0).
- For audio, prefer Opus at 48kHz. Around 96 kbps, 20 ms frame, and in-band FEC improve robustness against packet loss.
- If you still see stutter, increase the jitterbuffer latency on the receiver (at the cost of additional end-to-end latency).

## License
All files in this repository made by the author are copyrighted, and are protected by copyright laws and regulations in Japan and other jurisdictions.

Other files created by others are subject to their own license terms, which are described in the files themselves.