# テトリス（tetris.py）実行ガイド

7セグパネル上でテトリスを動かす Python スクリプト `tetris.py` の環境整備と実行手順です。

## 概要
- 端末（curses）で操作しつつ、I2C接続の7セグパネルに盤面を描画します。
- I2Cマルチプレクサ TCA9548A を使う構成／使わない直結構成の両方に対応します（`config.json` 参照）。

## 必要環境
- OS: Linux
- Python 3.8+
- pip（Pythonパッケージ管理）
- I2Cが有効化されたカーネルとデバイスノード（例: `/dev/i2c-0` もしくは `/dev/i2c-1`）
- Pythonパッケージ: `smbus2`
- (任意) I2Cツール: `i2c-tools`（`i2cdetect` でデバイス確認）

## I2Cの有効化と確認
1) I2Cを有効化（ボードごとの手順に従って設定ツールやDTを有効化）
2) デバイスノード確認
   ```bash
   ls -l /dev/i2c*
   # 例: /dev/i2c-0 があれば bus=0、/dev/i2c-1 があれば bus=1
   ```
3) (任意) I2Cスキャナ
   ```bash
   sudo apt-get update
   sudo apt-get install -y i2c-tools
   i2cdetect -l           # 利用可能なバス一覧
   i2cdetect -y 0         # 0番バスのデバイスを走査（環境に合わせて番号変更）
   ```

> 注意: `tetris.py` は既定で I2C バス番号 0 を使用します（`SMBus(0)`）。環境が `/dev/i2c-1` の場合は、`tetris.py` 内の `SMBus(0)` を `SMBus(1)` に変更してください。

## セットアップ手順（推奨: 仮想環境）
```bash
# リポジトリ直下で
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip
pip install smbus2
```

権限まわり
- `/dev/i2c-*` へアクセスできない場合は、次のいずれかを実施
  - 一時的に sudo で実行する
  - ユーザーを `i2c` グループへ追加し、再ログイン
    ```bash
    sudo usermod -aG i2c $USER
    # 反映のため一度ログアウト/ログイン
    ```

## 実行方法
- 基本:
  ```bash
  python tetris.py --config 16x12
  ```
- `--config` は `config.json` の `configurations` に定義された名前を指定します。
  - 例: `24x4`, `12x8`, `16x8`, `16x12`, `48x4`, `12x8-direct`
- 盤面サイズは設定により決まります（例: `16x12` は 16桁×12桁）。

## 操作方法（キーバインド）
- ← / →: 水平移動
- ↓: 1マス落下（自動落下タイマーをリセット）
- ↑ または Space: 回転
- q: 終了

## 設定ファイル（config.json）の要点
- `configurations` から構成名を選びます（`--config` で指定）。
- アドレス表記は 16進文字列（例: `"0x70"`）。スクリプト側で数値へ変換されます。
- TCA9548A を使う場合は `tca9548a_address` に 16進文字列（例: `"0x77"`）を設定。
- 直結構成は `tca9548a_address: null` とし、`channel_grids` のキーに `-1` を用います。
- 例: `16x12` 構成は 4桁×4桁モジュールを横4×縦3で合計 16×12 桁。

## トラブルシューティング
- I2C バスが見つからない（`I2C bus device not found`）
  - `/dev/i2c-*` の存在を確認。バス番号が 1 なら `SMBus(1)` に変更。
  - I2C を有効化したか確認。
- I/O エラー（`I2C Write Error` や 初期化失敗）
  - 配線、電源、アドレス（`i2cdetect` で確認）、TCA9548A のチャンネル切替を確認。
  - `config.json` の `channel_grids` と実配線が一致しているか確認。
- 端末が真っ黒／文字化け
  - リモート端末の幅が狭いと可視表示が折り返すことがあります。端末幅を確保。
- 終了時に表示が消えない
  - 例外時に消去できないケースがあります。再実行すると最終化処理で消去されます。

## 補足
- 既定の落下速度はスコアに応じて速くなります（ファイル先頭の定数で調整可能）。
- 描画は `'*'` と `'#'` を使っています（7セグ表示器へのマッピングは `digit_map` に定義）。

---
最小構成の実行例（16x12, バスが 0 の場合）
```bash
# 依存インストール（初回のみ）
python3 -m venv venv
source venv/bin/activate
pip install smbus2

# 実行
python tetris.py --config 16x12
```

バスが 1 の環境例（コード修正の必要あり）
```text
# tetris.py の該当行
- bus = SMBus(0)
+ bus = SMBus(1)
```
