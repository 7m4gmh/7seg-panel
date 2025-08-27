# 7-Segment LED Panel Video Player

[![Language](https://img.shields.io/badge/Language-C%2B%2B-blue.svg)](https://isocpp.org/)
[![Library](https://img.shields.io/badge/Library-OpenCV%20%7C%20GStreamer-green.svg)](https://opencv.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)](https://www.linux.org/)

This project provides a suite of tools for playing videos on a custom display panel composed of I2C-connected, HT16K33-based 7-segment LED modules. It supports flexible display configurations and automatically corrects the aspect ratio for video playback.

![Demo GIF](docs/demo.gif)
*(It is recommended to place an image or GIF demonstrating the project here)*

---

## ✨ Key Features

- **Flexible Display Configuration:**
  - Easily add or modify the physical layout of LED modules (e.g., horizontal, grid) by editing `config.h`.
  - Switch between configurations like `24x4` or `12x8` using command-line arguments.

- **Automatic Aspect Ratio Correction:**
  - Automatically crops source videos to match the aspect ratio of the selected display configuration.

- **Multiple Playback Modes:**
  1.  **HTTP Player (`7seg-http-player`):** A web server to upload videos and manage a playback queue from a browser.
  2.  **File Player (`7seg-file-player`):** A simple command-line tool to play a single video file directly.
  3.  **UDP Stream Player (`7seg-udp-player`):** A server that receives video/audio data via UDP and displays it in real-time.
  4.  **HTTP GStreamer Player (`7seg-http-streamer`):** An alternative HTTP server that uses GStreamer for video processing.

- **Modular Design:**
  - The core OpenCV-based video playback logic is centralized in `playback.cpp` for high maintainability.

## Hardware Requirements

- A Linux-based single-board computer with I2C capabilities (e.g., Raspberry Pi, Rockchip SBC).
- Multiple 7-segment LED modules featuring the HT16K33 LED driver IC.
- I2C bus wiring to connect the modules.

## Software Dependencies

- A C++17 compliant compiler (g++)
- `make`
- OpenCV 4 (`libopencv-dev`)
- GStreamer 1.0 (`libgstreamer1.0-dev`)
- `libi2c-dev` (for I2C communication)
- SDL2 (for audio playback in UDP streaming, `libsdl2-dev`)
- `cpp-httplib` (included in the project)

#### Installation Example on Ubuntu / Debian / Radxa OS
```bash
sudo apt update
sudo apt install -y build-essential make libopencv-dev libi2c-dev libsdl2-dev libgstreamer1.0-dev
```

## 🚀 How to Build

Run the `make` command to build all executables:
```bash
make
```
Upon success, four executables will be generated in the project's root directory.

## 📖 Usage

Each executable can optionally take a display configuration argument. If omitted, `24x4` is used by default.

### 1. HTTP Player (`7seg-http-player`)

Manage and play videos via a web UI. (Backend: OpenCV)

**Command:**
```bash
./7seg-http-player [default_video_dir] [config]
```
- `[default_video_dir]` (Optional): Path to the directory with default videos. (Default: `default_videos`)
- `[config]` (Optional): The display configuration to use. (e.g., `12x8`, Default: `24x4`)

**Example:**
```bash
./7seg-http-player default_videos 12x8
```
After starting, access the web UI from a PC on the same network at `http://<SBC_IP_ADDRESS>:8080`.

### 2. File Player (`7seg-file-player`)

Plays a single video file directly. (Backend: OpenCV)

**Command:**
```bash
./7seg-file-player <video_file> [config]
```
**Example:**
```bash
./7seg-file-player test.mp4 12x8
```

### 3. UDP Stream Player (`7seg-udp-player`)

Accepts and plays streaming data over the network.

**Command:**
```bash
./7seg-udp-player [config]
```
**Example:**
```bash
./7seg-udp-player 12x8
```
The server listens for UDP packets on port `9999`.

### 4. HTTP GStreamer Player (`7seg-http-streamer`)

An HTTP server that uses GStreamer as its backend, which may offer different performance characteristics or format support.

**Command:**
```bash
./7seg-http-streamer
```

## 📂 Project Structure

```
.
├── src/
│   ├── config.h
│   ├── playback.h/.cpp     # Common OpenCV playback engine
│   ├── http_player.cpp   # Main for 7seg-http-player
│   ├── file_player.cpp   # Main for 7seg-file-player
│   ├── udp_player.cpp    # Main for 7seg-udp-player
│   ├── http_streamer.cpp # Main for 7seg-http-streamer
│   └── ...
├── Makefile              # Build script
└── README.md
```