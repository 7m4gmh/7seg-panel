# 7段数码管LED面板视频播放器

[![Language](https://img.shields.io/badge/Language-C%2B%2B-blue.svg)](https://isocpp.org/)
[![Library](https://img.shields.io/badge/Library-OpenCV%20%7C%20GStreamer-green.svg)](https://opencv.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)](https://www.linux.org/)

本项目提供了一套工具，用于在由I2C连接的、基于HT16K33的7段数码管LED模块组成的自定义显示面板上播放视频。它支持灵活的显示配置，并能自动校正视频的宽高比。

![Demo GIF](docs/demo.gif)
*(建议在此处放置一个展示项目运行效果的图片或GIF)*

---

## ✨ 主要特性

- **灵活的显示配置:**
  - 通过编辑 `config.h` 文件，可以轻松添加或修改LED模块的物理布局（如水平长条形、网格形等）。
  - 启动时可通过命令行参数切换使用 `24x4` 或 `12x8` 等不同配置。

- **自动宽高比校正:**
  - 根据所选显示配置的宽高比，自动裁剪源视频，以确保正确的显示比例。

- **多种播放模式:**
  1.  **HTTP播放器 (`7seg-http-player`):** 一个Web服务器，允许您从浏览器上传视频并管理播放队列。
  2.  **文件播放器 (`7seg-file-player`):** 一个简单的命令行工具，用于直接播放单个视频文件。
  3.  **UDP流媒体播放器 (`7seg-udp-player`):** 通过UDP接收音视频数据，并实时显示在面板上。
  4.  **HTTP GStreamer播放器 (`7seg-http-streamer`):** 使用GStreamer进行视频处理的备用HTTP服务器。

- **模块化设计:**
  - 核心的、基于OpenCV的视频播放逻辑集中在 `playback.cpp` 中，确保了高可维护性和可重用性。

## 硬件要求

- 支持I2C功能的、基于Linux的单板计算机（如Raspberry Pi, Rockchip SBC等）。
- 多个搭载HT16K33 LED驱动IC的7段数码管LED模块。
- 用于连接模块的I2C总线接线。

## 软件依赖

- 支持C++17的编译器 (g++)
- `make`
- OpenCV 4 (`libopencv-dev`)
- GStreamer 1.0 (`libgstreamer1.0-dev`)
- `libi2c-dev` (用于I2C通信)
- SDL2 (用于UDP流媒体的音频播放, `libsdl2-dev`)
- `cpp-httplib` (已包含在项目内)

#### 在 Ubuntu / Debian / Radxa OS 上的安装示例
```bash
sudo apt update
sudo apt install -y build-essential make libopencv-dev libi2c-dev libsdl2-dev libgstreamer1.0-dev
```

## 🚀 如何构建

运行 `make` 命令来构建所有可执行文件：
```bash
make
```
成功后，项目根目录将生成四个可执行文件。

## 📖 使用方法

每个可执行文件都可以选择性地指定一个显示配置参数。如果省略，则默认使用 `24x4` 配置。

### 1. HTTP播放器 (`7seg-http-player`)

通过Web UI管理和播放视频。(后端: OpenCV)

**命令:**
```bash
./7seg-http-player [default_video_dir] [config]
```
- `[default_video_dir]` (可选): 存放默认视频的目录路径。(默认: `default_videos`)
- `[config]` (可选): 要使用的显示配置。(例如: `12x8`, 默认: `24x4`)

**示例:**
```bash
./7seg-http-player default_videos 12x8
```
启动后，从同一网络中的PC访问 `http://<SBC的IP地址>:8080`。

### 2. 文件播放器 (`7seg-file-player`)

直接播放单个视频文件。(后端: OpenCV)

**命令:**
```bash
./7seg-file-player <video_file> [config]
```
**示例:**
```bash
./7seg-file-player test.mp4 12x8
```

### 3. UDP流媒体播放器 (`7seg-udp-player`)

接收并播放在网络上传输的流媒体数据。

**命令:**
```bash
./7seg-udp-player [config]
```
**示例:**
```bash
./7seg-udp-player 12x8
```
服务器在端口 `9999` 上监听UDP数据包。

### 4. HTTP GStreamer播放器 (`7seg-http-streamer`)

一个使用GStreamer作为后端的HTTP服务器，可能提供不同的性能特性或格式支持。

**命令:**
```bash
./7seg-http-streamer
```

## 📂 项目结构

```
.
├── src/
│   ├── config.h
│   ├── playback.h/.cpp     # 通用OpenCV播放引擎
│   ├── http_player.cpp   # 7seg-http-player 的 main 函数
│   ├── file_player.cpp   # 7seg-file-player 的 main 函数
│   ├── udp_player.cpp    # 7seg-udp-player 的 main 函数
│   ├── http_streamer.cpp # 7seg-http-streamer 的 main 函数
│   └── ...
├── Makefile              # 构建脚本
└── README.md
```