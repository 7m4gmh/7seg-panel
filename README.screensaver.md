# LED Screensaver

7セグメントLEDパネル用のスクリーンセーバープログラムです。複数のアニメーションモードを備えています。

## 機能

- **IPアドレス表示**: 現在のIPアドレスをセンタリングして表示
- **Breakoutゲーム**: パドルでボールを跳ね返し、ブロックを破壊するゲーム
- **Invadersゲーム**: 砲台から弾を発射して敵を倒すゲーム
- **Miyajimaモード**: カウントアップアニメーション
- **Miyajima2モード**: カウントアップ + スクロールアニメーション
- **Clockモード**: 現在の時刻を表示

## インストール

依存パッケージのインストール:
```bash
pip install smbus2
```

## 実行方法

基本的な実行:
```bash
python screensaver.py
```

### コマンドラインオプション

- `--config CONFIG`: 設定ファイルの名前 (デフォルト: 16x12)
- `--mode MODE`: 固定モードで実行 (ip, breakout, invaders, miyajima, miyajima2, clock)
- `--test`: テスト表示モード

### 使用例

特定のモードで実行:
```bash
python screensaver.py --mode breakout
```

設定ファイルを指定:
```bash
python screensaver.py --config 48x8 --mode clock
```

テスト表示:
```bash
python screensaver.py --test
```

## 各モードの説明

### IPモード
現在のネットワークIPアドレスを取得して、ディスプレイの中央に表示します。

### Breakoutモード
古典的なブロック崩しゲームです：
- パドルが自動で左右に移動
- ボールが跳ね返る
- ブロックをすべて壊すとリセット

### Invadersモード
インベーダーゲーム風：
- 上部に敵が配置
- 下部に砲台があり、自動で弾を発射
- 敵をすべて倒すとリセット

### Miyajimaモード
カウントアップアニメーションを表示します。

### Miyajima2モード
カウントアップとスクロールアニメーションを組み合わせた表示です。

### Clockモード
現在の時刻をHH:MM:SS形式で表示します。

## 設定

`config.json`ファイルでディスプレイの構成を定義します。設定名を`--config`オプションで指定してください。

## システム要件

- Python 3.x
- smbus2ライブラリ
- I2C対応の7セグメントLEDパネル
- Raspberry PiなどのLinux環境

## ライセンス

このプロジェクトのライセンス情報を確認してください。