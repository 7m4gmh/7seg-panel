# 7セグメントLEDパネル ビデオプレイヤー

[![Language](https://img.shields.io/badge/Language-C%2B%2B-blue.svg)](https://isocpp.org/)
[![Library](https://img.shields.io/badge/Library-OpenCV%20%7C%20GStreamer-green.svg)](https://opencv.org/)
[![Platform](https://img.shields.io/badge/Platform-Linux-lightgrey.svg)](https://www.linux.org/)

このプロジェクトは、I2C接続されたHT16K33ベースの7セグメントLEDモジュールで構成されたカスタムディスプレイパネル上で、動画を再生するための一連のツールを提供します。柔軟なディスプレイ構成に対応しており、アスペクト比を自動で補正して映像を再生できます。

![Demo GIF](docs/demo.gif)
*(ここにプロジェクトの動作を示す画像かGIFを配置することを推奨します)*

---

## ✨ 主な特徴

- **柔軟なディスプレイ構成:**
  - `config.h` を編集することで、LEDモジュールの物理的な配置（横長、グリッド状など）を簡単に追加・変更できます。
  - 起動時の引数で `24x4` や `12x8` といった構成を切り替えて使用可能です。

- **アスペクト比の自動補正:**
  - 選択されたディスプレイ構成のアスペクト比に合わせて、ソース動画を自動でクロップ（切り抜き）し、正しい比率で表示します。

- **複数の再生モード:**
  1.  **HTTP Player (`7seg-http-player`):** Webブラウザから動画をアップロードし、再生キューを管理できるサーバーです。
  2.  **File Player (`7seg-file-player`):** コマンドラインから単一の動画ファイルを指定して直接再生するシンプルなツールです。
  3.  **UDP Stream Player (`7seg-udp-player`):** UDP経由で映像・音声データを受信し、リアルタイムでパネルに表示するサーバーです。
  4.  **HTTP GStreamer Player (`7seg-http-streamer`):** GStreamerを利用して映像処理を行う、代替のHTTPサーバーです。

- **モジュール化された設計:**
  - OpenCVベースの動画再生コアロジックは `playback.cpp` に集約されており、高いメンテナンス性と再利用性を実現しています。

## ハードウェア要件

- Raspberry Pi, Rockchip搭載SBCなど、I2Cが利用可能なLinuxベースのシングルボードコンピュータ
- HT16K33 LEDドライバICを搭載した7セグメントLEDモジュール (複数)
- 上記モジュールを接続するためのI2Cバス配線

## ソフトウェア依存関係

- C++17 対応のコンパイラ (g++)
- `make`
- OpenCV 4 (`libopencv-dev`)
- GStreamer 1.0 (`libgstreamer1.0-dev`)
- `libi2c-dev` (I2C通信用)
- SDL2 (UDPストリーミング時の音声再生用, `libsdl2-dev`)
- `cpp-httplib` (プロジェクト内に同梱)

#### Ubuntu / Debian / Radxa OS でのインストール例
```bash
sudo apt update
sudo apt install -y build-essential make libopencv-dev libi2c-dev libsdl2-dev libgstreamer1.0-dev
```

## 🚀 ビルド方法

1.  `make` コマンドを実行して、すべての実行ファイルをビルドします。
    ```bash
    make
    ```
    成功すると、プロジェクトのルートディレクトリに4つの実行ファイルが生成されます。

## 📖 使い方

各実行ファイルは、オプションでディスプレイ構成を指定できます。省略した場合は `24x4` がデフォルトで使用されます。

### 1. HTTP Player (`7seg-http-player`)

Web UI経由で動画を管理・再生します。（バックエンド: OpenCV）

**コマンド:**
```bash
./7seg-http-player [default_video_dir] [config]
```
- `[default_video_dir]` (オプション): デフォルト動画が格納されているディレクトリのパス。 (デフォルト: `default_videos`)
- `[config]` (オプション): 使用するディスプレイ構成。 (例: `12x8`, デフォルト: `24x4`)

**実行例:**
```bash
./7seg-http-player default_videos 12x8
```
起動後、SBCと同じネットワーク内のPCから `http://<SBCのIPアドレス>:8080` にアクセスします。

### 2. File Player (`7seg-file-player`)

単一の動画ファイルを直接再生します。（バックエンド: OpenCV）

**コマンド:**
```bash
./7seg-file-player <video_file> [config]
```
**実行例:**
```bash
./7seg-file-player test.mp4 12x8
```

### 3. UDP Stream Player (`7seg-udp-player`)

ネットワーク経由でストリーミングデータを受け付けて再生します。

**コマンド:**
```bash
./7seg-udp-player [config]
```
**実行例:**
```bash
./7seg-udp-player 12x8
```
サーバーはポート `9999` でUDPパケットを待ち受けます。

### 4. HTTP GStreamer Player (`7seg-http-streamer`)

GStreamerをバックエンドとして利用するHTTPサーバーです。異なるパフォーマンス特性やフォーマット対応が期待できます。

**コマンド:**
```bash
./7seg-http-streamer
```

## 📂 プロジェクト構造

```
.
├── src/
│   ├── config.h
│   ├── playback.h/.cpp     # 共通のOpenCV再生エンジン
│   ├── http_player.cpp   # 7seg-http-player の main
│   ├── file_player.cpp   # 7seg-file-player の main
│   ├── udp_player.cpp    # 7seg-udp-player の main
│   ├── http_streamer.cpp # 7seg-http-streamer の main
│   └── ...
├── Makefile              # ビルドスクリプト
└── README.md
```