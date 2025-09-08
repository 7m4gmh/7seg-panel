# 7セグメントLEDパネル プレイヤー

## 概要

このプロジェクトは、自作の大型7セグメントLEDパネルアレイで動画を再生するための総合的なツール群です。リアルタイムで動画ファイルを処理し、TCA9548A I2Cマルチプレクサを介して複数のI2CベースLEDモジュールを駆動する信号に変換します。

堅牢なエラー検知・復旧メカニズムを搭載しており、不安定なI2C通信が発生しても自動で復旧を試み、安定した長時間運用を可能にします。

## 目次

- [概要](#概要)
- [主な機能](#主な機能)
- [前提条件](#前提条件)
- [ビルド方法](#ビルド方法)
- [設定方法](#設定方法)
- [使用方法](#使用方法)
  - [ファイルプレイヤー (7seg-file-player)](#ファイルプレイヤー-7seg-file-player)
  - [HTTPプレイヤー (7seg-http-player)](#httpプレイヤー-7seg-http-player)
  - [RTPプレイヤー（推奨） (7seg-rtp-player)](#rtpプレイヤー推奨-7seg-rtp-player)
  - [UDPプレイヤー (7seg-udp-player)](#udpプレイヤー-7seg-udp-player)
- [トラブルシューティング](#トラブルシューティング)
  - [RTP受信に関するヒント](#rtp受信に関するヒント)
- [ライセンス](#ライセンス)

## 主な機能

- **多彩なプレイヤー**:
  - `7seg-file-player`: ローカルの動画ファイルを再生します。
  - `7seg-http-player`: Web UIを提供し、動画のアップロード、キュー管理、再生制御が可能です。
  - `7seg-udp-player`: UDPストリームを受信してリアルタイム表示します。
- **柔軟なパネル構成**: `config.json`により、LEDモジュールの物理的な配置やI2Cアドレスを自由に定義できます。
- **高度なI2Cエラー復旧**:
  - 通信エラーを自動で検知し、ディスプレイの再初期化を実行します。
  - 復旧処理が失敗した場合、成功するまで複数回リトライします。
- **ハードウェア問題分析**: プログラム終了時(`Ctrl+C`)に、エラーが発生したI2Cチャンネルとアドレスごとの回数を集計・表示し、問題のあるハードウェア（モジュール、配線）の特定を支援します。
- **Web UIによる遠隔操作** (`7seg-http-player`):
  - 動画ファイルのアップロード
  - 再生キューの管理（追加・削除）
  - 現在再生中の動画の停止
  - 再生状況の確認

## 前提条件

- C++17対応コンパイラ (`g++`)
- `make`
- **OpenCV 4** (`libopencv-dev`)
- **libSDL2** (`libsdl2-dev`) (オーディオ再生用)
- **GStreamer** (標準入力からのストリーミング再生用)

## ビルド方法

See [README.md](README.md)

## 設定方法

`config.json`ファイルを編集して、お使いのLEDパネルのハードウェア構成を定義します。

**設定例 (`16x16_expanded`):**

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

## 使用方法

### ファイルプレイヤー (`7seg-file-player`)

ローカルの動画ファイルを再生します。

```bash
./7seg-file-player <動画ファイルのパス> [設定名]
```
例: `./7seg-file-player ./videos/my_video.mp4 16x16_expanded`

### HTTPプレイヤー (`7seg-http-player`)

Webサーバーを起動し、ブラウザから操作します。

```bash
./7seg-http-player <デフォルト動画のディレクトリパス> [設定名]
```

例: `./7seg-http-player ./default_videos 16x16_expanded`

サーバー起動後、Webブラウザで `http://<ラズベリーパイのIPアドレス>:8080` にアクセスしてください。

### RTPプレイヤー（推奨） (`7seg-rtp-player`)

RTPで受信することでジッタ吸収が強く、ネットワーク環境でも安定しやすくなります。

```bash
./7seg-rtp-player [設定名] [映像ポート] [音声ポート]
```
例: `./7seg-rtp-player 16x12 9999 10000`

受信を開始したら、送信側（PCやMac）からH.264（pt=96）とOpus（pt=97）のRTPを同一ホストへ送信してください。

#### 送信（GStreamer: ファイル→RTP）
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

#### 送信（macOS: カメラ/マイク→RTP）
macにGStreamerが入っていれば、同梱スクリプトで簡単に送出できます。

```bash
./send_rtp_cam_gst.sh ROCK_IP 0 0 9999 10000
```
- 自動で利用可能なエンコーダ/音声ソースを判定します（VideoToolbox優先、なければx264enc; avfaudiosrc→osxaudiosrc→autoaudiosrcの順でフォールバック）。
- 既定は 320x240 / 15fps / H.264 約300kbps、音声は Opus 48kHz 96kbps + FEC。

#### 送信（mac/PC: ファイル→RTP）
同梱のファイル送信用スクリプトもあります。

```bash
./send_rtp_test_gst.sh [INPUT] [HOST] [VPORT] [APORT]
# 例
./send_rtp_test_gst.sh ../led/mtknsmb2.mp4 192.168.10.107 9999 10000
```

> ヒント: 有線LANのほうが安定します。映像がカクつく場合はビットレートを下げる/フレームレートや解像度を下げる、音が途切れる場合は送受信の遅延（jitterbuffer）を少し増やしてください。

### UDPプレイヤー (`7seg-udp-player`)

指定したポートでUDPストリームを待ち受けます。

```bash
./7seg-udp-player <ポート番号> [設定名]
```
例: `./7seg-udp-player 12345 16x16_expanded`

### FLV(TCP)→TS(UDP) ブリッジ（安定運用向け）

OBSからFLV(TCP)で送ったものを、ローカルでMPEG-TS(UDP)に変換して再生する手順です。

1) プレイヤ（TS受信）
```bash
./7seg-net-player ts 24x4 5004
```

2) ブリッジ（FLV→TS）
```bash
./flv_listen_to_ts.sh 5005 5004
```

3) OBS（録画タブ→カスタム出力/FFmpeg）
- コンテナ: flv、映像: H.264、音声: AAC
- URL: tcp://<受信機IP>:5005


## トラブルシューティング

I2C通信エラーが頻繁に発生する場合、プログラムは自動で復旧を試みます。`Ctrl+C`でプログラムを終了すると、エラーの発生源を特定するための分析レポートが表示されます。

**分析レポートの例:**

```
--- I2C Error Analysis ---
Channel: 2, Address: 0x70 => 15 errors
Channel: 2, Address: 0x72 => 42 errors
```

この例では、エラーがTCA9548Aの**チャンネル2**に集中していることがわかります。これは、チャンネル2の配線、またはそのチャンネルに接続されたモジュール(`0x70`, `0x72`)に物理的な問題がある可能性が高いことを示唆しています。

### RTP受信に関するヒント
- 受信側のRTPポートはデフォルトで映像9999/音声10000を例示しています。環境に合わせて変更してください。
- 送信側の映像は 320x240 / 15fps / H.264（keyframe=30, zerolatency, bframes=0）程度が安定です。
- 音声は Opus 48kHz を推奨。96kbps前後・20ms frame・in-band FEC を有効にするとロスに強くなります。
- さらに安定させたい場合は、受信側のジッタバッファ遅延（rtpjitterbuffer latency）を少し上げると効果があります（遅延は増えます）。

## ライセンス

このプロジェクトの著作権は、作者に帰属し、日本およびその他の法域における著作権に関する法令により保護されます。

自作でない一部のソースコードについては、当該ソースコードの先頭部分にライセンスの表記があります。
