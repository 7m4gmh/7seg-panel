# RPi5でのテスト手順

このドキュメントでは、差分レンダリング最適化を実装した7-Segment LED PanelソフトウェアをRaspberry Pi 5でテストする方法を説明します。

## 前提条件

- Raspberry Pi 5 (RPi5)
- Raspberry Pi OS (64-bit) がインストール済み
- SSH接続が可能
- インターネット接続が可能

## テスト手順

### 1. ソースコードの転送

macOSからRPi5へソースコードを転送します：

```bash
# macOS側で圧縮
cd "/Users/hijiri/Library/Mobile Documents/com~apple~CloudDocs/0000 prototyping/20250821-7seg-led-panel-software/7seg-panel"
tar -czf 7seg-panel.tar.gz .

# RPi5へ転送 (RPi5のIPアドレスに合わせて変更してください)
scp 7seg-panel.tar.gz pi@192.168.1.xxx:~
```

### 2. RPi5での展開とビルド

RPi5上で以下のコマンドを実行：

```bash
# ファイルの展開
tar -xzf 7seg-panel.tar.gz
cd 7seg-panel

# ビルドスクリプトの実行
./build_rpi5.sh
```

### 3. テスト結果の確認

ビルドスクリプトは以下のテストを自動的に実行します：

1. **パフォーマンスベンチマーク**
   - 1000フレームのレンダリング時間を測定
   - FPSと平均フレーム時間を表示

2. **エミュレータモードテスト**
   - ファイルプレイヤーをエミュレータモードで実行
   - 10秒間テスト（タイムアウト）

## 期待される結果

### パフォーマンス比較

現在の最適化版（差分レンダリング）:
- **FPS: ~60.86** (macOSでの測定)
- **フレーム時間: ~16.43ms**

RPi5での期待値:
- FPS: 50-70程度（ハードウェア性能による）
- 差分レンダリングにより、毎回全セグメント再描画する場合より大幅に高速

### テスト成功の兆候

1. ビルドが成功し、バイナリが生成される
2. ベンチマークが正常に実行され、FPSが表示される
3. エミュレータモードでウィンドウが表示される（GUI環境の場合）

## トラブルシューティング

### 依存関係のインストールエラー

```bash
# パッケージリストの更新
sudo apt update

# 個別インストール
sudo apt install libopencv-dev
sudo apt install libsdl2-dev
sudo apt install libgstreamer1.0-dev
```

### コンパイルエラー

```bash
# メモリ不足の場合
make -j1  # 並列ビルドを無効化

# 詳細なエラー確認
make 2>&1 | tee build.log
```

### GUI表示の問題

RPi5でGUIが表示されない場合：

```bash
# X11フォワーディングの確認
echo $DISPLAY

# またはヘッドレスモードでテスト
export DISPLAY=:0
```

## 結果の報告

テスト結果を以下の形式で報告してください：

```
=== RPi5 Test Results ===

Build: SUCCESS/FAILED
Benchmark FPS: XX.XX
Benchmark Frame Time: XX.XX ms
Emulator Test: SUCCESS/FAILED

Hardware: Raspberry Pi 5
OS: Raspberry Pi OS 64-bit
OpenCV Version: X.X.X
SDL Version: X.X.X

Additional Notes:
- [テスト時の特記事項]
```

## 次のステップ

1. RPi5でのテストが成功したら、物理的なLEDパネルでのテスト
2. 必要に応じてさらなる最適化
3. 実世界でのビデオ再生テスト

---

**注意**: このテストはエミュレータモードのみを対象としています。物理的なLEDパネルを使用する場合は、別途I2C設定とハードウェア接続が必要です。
