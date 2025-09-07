# 7段顯示器LED面板 播放器

## 專案簡介

本專案是一套完整的工具組，專為在自製的大型7段顯示器LED面板陣列上播放影片而設計。它能即時處理影片檔案，將其轉換為驅動訊號，並透過 TCA9548A I2C 多工器來控制多個基於 I2C 的 LED 模組。

專案內建了穩健的錯誤偵測與恢復機制，當 I2C 通訊不穩定時，程式會自動嘗試恢復，以實現穩定且長時間的運作。

## 目錄

- [專案簡介](#專案簡介)
- [主要功能](#主要功能)
- [系統需求](#系統需求)
- [如何編譯](#如何編譯)
- [設定方法](#設定方法)
- [使用方法](#使用方法)
  - [檔案播放器 (7seg-file-player)](#檔案播放器-7seg-file-player)
  - [HTTP 播放器 (7seg-http-player)](#http-播放器-7seg-http-player)
  - [RTP 播放器（建議） (7seg-rtp-player)](#rtp-播放器建議-7seg-rtp-player)
  - [UDP 播放器 (7seg-udp-player)](#udp-播放器-7seg-udp-player)
- [問題排解](#問題排解)
  - [RTP 接收提示](#rtp-接收提示)
- [授權條款](#授權條款)

## 主要功能

- **多種播放模式**:
  - `7seg-file-player`: 播放本地影片檔案。
  - `7seg-http-player`: 提供 Web UI，可用於上傳影片、管理播放佇列和控制播放。
  - `7seg-udp-player`: 接收並即時顯示 UDP 串流。
- **彈性的面板配置**: 可透過 `config.json` 檔案自由定義 LED 模組的實體佈局和 I2C 位址。
- **進階的 I2C 錯誤恢復**:
  - 自動偵測通訊錯誤並重新初始化顯示器。
  - 若初次恢復失敗，會進行多次重試直到成功為止。
- **硬體問題分析**: 當程式結束時 (透過 `Ctrl+C`)，會顯示按 I2C 通道和位址分類的錯誤計數報告，協助使用者定位有問題的硬體（模組、線路）。
- **透過 Web UI 遠端控制** (`7seg-http-player`):
  - 上傳影片檔案。
  - 管理播放佇列（新增/刪除）。
  - 停止當前播放的影片。
  - 查看播放狀態。

## 系統需求

- 支援 C++17 的編譯器 (`g++`)
- `make`
- **OpenCV 4** (`libopencv-dev`)
- **libSDL2** (`libsdl2-dev`) (用於音訊播放)
- **GStreamer** (用於從標準輸入進行串流播放)

## 如何編譯

See [README.md](README.md)

## 設定方法

編輯 `config.json` 檔案以定義您的 LED 面板硬體配置。

**設定範例 (`16x16_expanded`):**
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

- `tca9548a_address`: TCA9548A I2C 多工器的位址（十進位）。若不使用，請設為 `-1`。
- `total_width`, `total_height`: 面板整體的寬度和高度（以字元數計算）。
- `module_digits_width`, `module_digits_height`: 單一 LED 模組的寬度和高度（以字元數計算）。
- `channel_grids`: 以二維陣列的形式，定義連接到 TCA9548A 各個通道的模組 I2C 位址。

## 使用方法

### 檔案播放器 (`7seg-file-player`)

播放本地影片檔案。

```bash
./7seg-file-player <影片檔案路徑> [設定名稱]
```
範例: `./7seg-file-player ./videos/my_video.mp4 16x16_expanded`

### HTTP 播放器 (`7seg-http-player`)

啟動一個 Web 伺服器，可透過瀏覽器進行操作。

```bash
./7seg-http-player <預設影片目錄路徑> [設定名稱]
```
範例: `./7seg-http-player ./default_videos 16x16_expanded`

伺服器啟動後，請在瀏覽器中訪問 `http://<您的裝置IP位址>:8080`。

### RTP 播放器（建議） (`7seg-rtp-player`)

RTP 具備更好的網路抖動處理能力，通常在一般網路環境下較為穩定。

```bash
./7seg-rtp-player [設定名稱] [視訊連接埠] [音訊連接埠]
```
範例: `./7seg-rtp-player 16x12 9999 10000`

啟動接收端後，請從傳送端將 H.264（pt=96）與 Opus（pt=97）的 RTP 串流送到相同主機。

#### 傳送（GStreamer：檔案 → RTP）
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

#### 傳送（macOS：相機/麥克風 → RTP）
若您的 Mac 已安裝 GStreamer，可使用隨附腳本快速送出：

```bash
./send_rtp_cam_gst.sh ROCK_IP 0 0 9999 10000
```
- 會自動偵測可用的編碼器與音源（VideoToolbox 優先，否則退回 x264enc；音源 avfaudiosrc → osxaudiosrc → autoaudiosrc 順序）。
- 預設為 320x240 / 15fps、H.264 約 300kbps，音訊為 Opus 48kHz 96kbps 並啟用 FEC。

#### 傳送（mac/PC：檔案 → RTP）
亦提供檔案傳送用腳本：

```bash
./send_rtp_test_gst.sh [INPUT] [HOST] [VPORT] [APORT]
# 範例
./send_rtp_test_gst.sh ../led/mtknsmb2.mp4 192.168.10.107 9999 10000
```

> 提示：有線網路通常更穩定。若畫面卡頓，請降低視訊位元率/幀率/解析度；若音訊斷續，請適度提高接收與傳送端的延遲（例如增加 jitterbuffer latency）。

### UDP 播放器 (`7seg-udp-player`)

在指定的連接埠上監聽 UDP 串流。

```bash
./7seg-udp-player <連接埠號碼> [設定名稱]
```
範例: `./7seg-udp-player 12345 16x16_expanded`

## 問題排解

如果您遇到頻繁的 I2C 通訊失敗，程式會自動嘗試恢復。當您使用 `Ctrl+C` 結束程式時，會顯示一份分析報告以協助您定位錯誤來源。

**分析報告範例:**
```
--- I2C Error Analysis ---
Channel: 2, Address: 0x70  => 15 errors
Channel: 2, Address: 0x72  => 42 errors
--------------------------
```
此範例顯示錯誤集中在 TCA9548A 的**通道 2**。這強烈暗示問題很可能出在通道 2 的線路，或是連接到該通道的模組（`0x70`, `0x72`）上。

### RTP 接收提示
- 範例使用視訊 9999 / 音訊 10000 作為連接埠，可依環境調整。
- 建議先以 320x240 / 15fps / H.264（keyframe=30、zerolatency、bframes=0）作為穩定起點。
- 音訊建議使用 Opus 48kHz，約 96 kbps、20 ms frame，並啟用 in-band FEC，以提升對封包遺失的容忍度。
- 若仍有卡頓，請提高接收端的 jitterbuffer latency（將增加端到端延遲）。

## 授權條款

本專案的著作權歸作者所有，並受日本及其他地區的著作權相關法律保護。

關於非由作者原創的原始碼，其授權條款標示於該原始碼的開頭部分。