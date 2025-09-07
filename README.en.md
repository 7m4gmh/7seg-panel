# 7-Segment LED Panel Player

## Overview

This project is a comprehensive suite of tools designed to play videos on a custom-built large-scale 7-segment LED panel array. It processes video files in real-time, converting them into signals to drive multiple I2C-based LED modules, managed by a TCA9548A I2C multiplexer.

It features a robust error detection and recovery mechanism, allowing for stable, long-term operation by automatically attempting to recover from unstable I2C communication.

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

### 1. File Player (`7seg-file-player`)

Plays a local video file.

```bash
./7seg-file-player <path_to_video_file> [config_name]
```
Example: `./7seg-file-player ./videos/my_video.mp4 16x16_expanded`

### 2. HTTP Player (`7seg-http-player`)

Starts a web server for browser-based control.

```bash
./7seg-http-player <path_to_default_videos_dir> [config_name]
```
Example: `./7seg-http-player ./default_videos 16x16_expanded`

After starting the server, access the UI by navigating to `http://<your_pi_ip_address>:8080` in your web browser.

### 3. UDP Player (`7seg-udp-player`)

Listens for a UDP stream on a specified port.

```bash
./7seg-udp-player <port_number> [config_name]
```
Example: `./7seg-udp-player 12345 16x16_expanded`

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

## License
All files in this repository made by the author are copyrighted, and are protected by copyright laws and regulations in Japan and other jurisdictions.

Other files created by others are subject to their own license terms, which are described in the files themselves.