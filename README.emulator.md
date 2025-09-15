# 7-Segment LED Panel Emulator

macOS上で7セグメントLEDパネルの動作をエミュレートするソフトウェアです。物理的なLEDパネルの正確な外観と動作をシミュレートします。

## 概要

このエミュレータは、i2cで動作する物理的な7セグメントLEDパネルを、macOS環境で視覚的に再現するためのツールです。OpenCVを使用して物理的なLEDの形状、傾き、配置を正確にシミュレートします。

## 主な特徴

### 🎯 正確な物理シミュレーション
- **7セグメントLED**: 物理的な7セグメントLEDの形状を正確に再現
- **スケール調整**: 8倍スケールで視認性を確保しつつ物理比率を維持

### ⚡ 高性能最適化
- **プリキャッシュ**: 起動時に全セグメントの座標を計算してメモリに格納
- **音声同期**: 動画再生時の音声との同期を最適化

### 🎮 使いやすいインターフェース
- **自動モード検出**: config.jsonから自動的にエミュレータモードを検出
- **リアルタイム表示**: OpenCVウィンドウでリアルタイムにLED表示
- **キーボード制御**: ESCキーで再生停止

## 必要条件

### システム要件
- **macOS** (10.15以上推奨)
- **C++17** 対応コンパイラ
- **OpenCV 4** (Homebrewでインストール)

### 依存関係のインストール
```bash
# HomebrewでOpenCVをインストール
brew install opencv

# SDL2もインストール（音声再生用）
brew install sdl2
```

## ビルド方法

### 完全ビルド
```bash
# すべてのターゲットをビルド
make all
```

### エミュレータ専用ビルド
```bash
# コアプレイヤーのみビルド（エミュレータを含む）
make core
```

## 使用方法

### 基本的な使い方

1. **設定ファイルの準備**
   ```json
   {
     "type": "emulator",
     "total_width": 24,
     "total_height": 4,
     "configurations": {
       "emulator-24x4": {
         "total_width": 24,
         "total_height": 4
       }
     }
   }
   ```

2. **動画ファイルの再生**
   ```bash
   # 設定ファイルで指定した構成で再生
   ./7seg-file-player video.mp4 emulator-24x4

   # または直接設定
   ./7seg-file-player video.mp4 emulator-20x20
   ```

### 設定オプション

#### ディスプレイサイズ
- `emulator-24x4`: 24桁 × 4行（標準サイズ）
- `emulator-20x20`: 20桁 × 20行（大画面）
- カスタムサイズも指定可能

#### 設定ファイル (config.json)
```json
{
  "type": "emulator",
  "total_width": 24,
  "total_height": 4,
  "configurations": {
    "emulator-24x4": {
      "total_width": 24,
      "total_height": 4
    }
  }
}
```

### 再生制御

- **ESCキー**: 再生停止
- **Ctrl+C**: プログラム終了

## アーキテクチャ

### セグメント描画アルゴリズム

エミュレータは物理的な7セグメントLEDの正確な形状を再現するために、以下の計算を行っています：

1. **六角形セグメント生成**
   ```cpp
   // 横セグメントの六角形形状
   std::vector<cv::Point> horizontal_segment(double x, double y) {
       // 六角形の頂点座標を計算
   }
   ```

2. **縦セグメントの回転**
   ```cpp
   // 横セグメントを90±10度回転
   std::vector<cv::Point> vertical_segment(double x, double y, double tilt_deg)
   ```

3. **レイアウト計算**
   ```cpp
   // 全セグメントの最終位置を計算
   SegmentLayout make_layout(int digit_idx, double package_center_x, double package_center_y)
   ```

### パフォーマンス最適化

#### プリキャッシュシステム
```cpp
struct CachedDisplayLayout {
    std::vector<std::vector<std::vector<cv::Point>>> all_segs; // 全桁のセグメント
    std::vector<cv::Point> all_dp_centers; // 小数点位置
    std::vector<int> all_dp_radii; // 小数点半径
    std::vector<std::pair<double, double>> package_centers; // パッケージ中心座標
};
```

#### 音声同期
- 動画フレームの理想タイミングを計算
- 遅延が発生した場合のフレームスキップ処理
- 音声再生との同期維持

## 技術仕様

### 物理パラメータ
- **パッケージ幅**: 12.7mm
- **パッケージ高**: 19.05mm
- **セグメント長**: 6.0mm
- **セグメント幅**: 2.0mm
- **傾き角度**: 10度
- **表示スケール**: 8倍

### 色設定
- **点灯セグメント**: 赤 (BGR: 0, 0, 255)
- **消灯セグメント**: 灰色 (BGR: 80, 80, 80)
- **背景**: 黒 (BGR: 0, 0, 0)

## トラブルシューティング

### ビルドエラー
```bash
# OpenCVが見つからない場合
brew install opencv

# パスが通っていない場合
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
```

### 実行時エラー
```bash
# 動画ファイルが見つからない
ls -la video.mp4

# 設定ファイルの確認
cat config.json
```

### パフォーマンス問題
- **遅延が大きい**: ディスプレイサイズを小さくする
- **CPU使用率が高い**: より高性能なマシンを使用
- **メモリ不足**: ディスプレイサイズを調整

## 開発情報

### ソースコード構造
```
src/
├── playback.cpp          # メイン再生ロジック
├── emulator_display.cpp  # エミュレータ表示クラス（オリジナル）
└── common.h             # 共通定義
```

### 主要関数
- `play_video_stream_emulator()`: エミュレータ再生メイン関数
- `create_cached_layout()`: レイアウトプリキャッシュ関数
- `make_layout()`: 個別セグメントレイアウト計算
- `horizontal_segment()`: 横セグメント形状生成
- `vertical_segment()`: 縦セグメント形状生成

## 関連リンク

- [メインREADME](README.md)
- [英語版README](README.en.md)
- [日本語版README](README.ja.md)
- [テトリス版README](README.tetris.md)

## ライセンス

ライセンスについては考え中。ひとまず留保します。個人的に使っていただく分には自由です。