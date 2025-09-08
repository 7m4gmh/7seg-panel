# 俄羅斯方塊（tetris.py）執行指南

本文件說明如何準備環境並執行 `tetris.py`，在 7 段顯示器 LED 面板上運行俄羅斯方塊。

## 概述
- 透過終端機（curses）操作，同步在 I2C 連接的 7 段顯示器面板上繪製棋盤。
- 同時支援使用 TCA9548A I2C 多工器的組態與不使用多工器的直連組態（見 `config.json`）。

## 系統需求
- OS: Linux
- Python 3.8+
- pip（Python 套件管理）
- 已啟用 I2C 並存在裝置節點（例如 `/dev/i2c-0` 或 `/dev/i2c-1`）
- Python 套件：`smbus2`
- （選用）I2C 工具：`i2c-tools`（用 `i2cdetect` 驗證裝置）

## 啟用並確認 I2C
1) 啟用 I2C（依您的開發板設定工具 / DT 流程）
2) 檢查裝置節點
   ```bash
   ls -l /dev/i2c*
   # 範例：若存在 /dev/i2c-0 則 bus=0；若存在 /dev/i2c-1 則 bus=1
   ```
3) （選用）I2C 掃描
   ```bash
   sudo apt-get update
   sudo apt-get install -y i2c-tools
   i2cdetect -l           # 列出可用的匯流排
   i2cdetect -y 0         # 掃描 0 號匯流排（依環境修改編號）
   ```

> 注意：`tetris.py` 預設使用 I2C 匯流排 0（`SMBus(0)`）。若系統使用 `/dev/i2c-1`，請將 `tetris.py` 內的 `SMBus(0)` 改為 `SMBus(1)`。

## 安裝步驟（建議：虛擬環境）
```bash
# 於倉庫根目錄
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip
pip install smbus2
```

權限
- 若無法存取 `/dev/i2c-*`，請：
  - 暫時以 sudo 執行，或
  - 將使用者加入 `i2c` 群組並重新登入：
    ```bash
    sudo usermod -aG i2c $USER
    # 重新登入以套用
    ```

## 執行方式
- 基本：
  ```bash
  python tetris.py --config 16x12
  ```
- `--config` 需為 `config.json` 之 `configurations` 下的名稱。
  - 範例：`24x4`, `12x8`, `16x8`, `16x12`, `48x4`, `12x8-direct`
- 棋盤尺寸由設定決定（例如 `16x12` → 16×12 位數）。

## 操作方式（快捷鍵）
- ← / →：水平移動
- ↓：下降一格（重設自動下落計時）
- ↑ 或 Space：旋轉
- q：結束

## 設定檔（config.json）重點
- 從 `configurations` 中選擇組態名稱，並以 `--config` 指定。
- 位址以十六進位字串表示（例如 `"0x70"`），腳本內會轉為數值。
- 使用 TCA9548A 時，`tca9548a_address` 請填十六進位字串（例如 `"0x77"`）。
- 直連組態請設 `tca9548a_address: null`，並在 `channel_grids` 中使用 `-1` 作為鍵。
- 例：`16x12` 使用 4×4 位數模組，橫向 4、縱向 3，合計 16×12 位數。

## 疑難排解
- 找不到 I2C 匯流排（`I2C bus device not found`）
  - 確認 `/dev/i2c-*` 存在。若為匯流排 1，請改成 `SMBus(1)`。
  - 確認系統已啟用 I2C。
- I/O 錯誤（`I2C Write Error` 或初始化失敗）
  - 檢查接線、電源、位址（用 `i2cdetect`）、TCA9548A 通道切換。
  - `config.json` 的 `channel_grids` 是否與實際接線相符。
- 終端機畫面全黑或亂碼
  - 若寬度不足，ASCII 繪圖會換行；請確保足夠寬度。
- 結束時面板未清除
  - 少數例外狀況清理可能失敗；再次執行通常會清除。

## 補充
- 自動下落速度會隨分數提升（可於檔案開頭的常數調整）。
- 棋盤繪圖使用 `'*'` 與 `'#'`（對應 7 段顯示器的 `digit_map`）。

---
最小範例（16x12，匯流排 0）
```bash
# 第一次安裝依賴
python3 -m venv venv
source venv/bin/activate
pip install smbus2

# 執行
python tetris.py --config 16x12
```

匯流排 1（需修改程式碼）
```text
# 於 tetris.py
- bus = SMBus(0)
+ bus = SMBus(1)
```
