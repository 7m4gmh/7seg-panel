# config.json の説明

## 概要

`config.json` は、7seg-panel のディスプレイ構成を定義する設定ファイルです。このファイルでは、LED パネルの物理的な接続構成を JSON 形式で記述します。主に TCA9548A I2C マルチプレクサを使用した複雑な接続をサポートしています。

## 基本構造

```json
{
  "comment_char_dimensions": "LED character dimensions in mm",
  "char_width_mm": 12.7,
  "char_height_mm": 19.2,

  "comment_configs": "Display panel configurations",
  "configurations": {
    "config_name": {
      "name": "Display Name",
      "buses": {
        "bus_id": {
          "tca9548as": [
            {
              "address": "0x77",
              "channels": {
                "channel_id": [
                  ["0x70", "0x71"],
                  ["0x72", "0x73"]
                ]
              }
            }
          ]
        }
      },
      "module_digits_width": 4,
      "module_digits_height": 4,
      "total_width": 8,
      "total_height": 8
    }
  }
}
```

## フィールドの詳細

### ルートレベル

- `char_width_mm`: 7セグメントLEDのパッケージの幅 (mm)
- `char_height_mm`: L7セグメントLEDのパッケージの高さ (mm)
- `configurations`: 各構成の定義

### configurations の各エントリ

- `name`: 構成の表示名
- `type`: 構成のタイプ ("physical" または "emulator") - オプション、デフォルトは "physical"
- `buses`: Bus の定義 (オブジェクト)
- `module_digits_width`: 各モジュールの桁数 (幅)
- `module_digits_height`: 各モジュールの桁数 (高さ)
- `total_width`: 全体の桁数 (幅)
- `total_height`: 全体の桁数 (高さ)

### buses

Bus ID をキーとするオブジェクト。各 Bus は以下の構造を持ちます：

- `tca9548as`: TCA9548A の配列

### tca9548as の各エントリ

- `address`: TCA9548A の I2C アドレス (16進数文字列、例: "0x77") または null (直接接続)
- `channels`: チャンネルの定義 (オブジェクト)
- `rows`: 行ベースのレイアウト定義 (オブジェクト、オプション)

### channels

チャンネル ID をキーとするオブジェクト。各チャンネルは LED モジュールのグリッド (2D 配列) を値とします。

- グリッドは行の配列
- 各行はモジュールアドレスの配列 (16進数文字列)

### rows (48x8 構成用)

行 ID をキーとするオブジェクト。各行は以下の構造を持ちます：

- `channel`: 使用する TCA9548A チャンネル ID
- `row_offset`: 行のオフセット (桁単位)
- `col_offset`: 列のオフセット (桁単位)

この設定は、異なるチャンネルが同じ行に配置される複雑なレイアウトで使用します。

## 例

### 単純な構成 (24x4)

```json
{
  "configurations": {
    "24x4": {
      "name": "24x4 Horizontal",
      "buses": {
        "0": {
          "tca9548as": [
            {
              "address": "0x77",
              "channels": {
                "0": [
                  ["0x70", "0x71", "0x72", "0x73", "0x74", "0x75"]
                ]
              }
            }
          ]
        }
      },
      "module_digits_width": 4,
      "module_digits_height": 4,
      "total_width": 24,
      "total_height": 4
    }
  }
}
```

### 複数の TCA9548A を使用した構成

```json
{
  "configurations": {
    "12x8": {
      "name": "12x8 Expanded",
      "buses": {
        "0": {
          "tca9548as": [
            {
              "address": "0x77",
              "channels": {
                "0": [
                  ["0x70", "0x71", "0x72"]
                ]
              }
            },
            {
              "address": "0x76",
              "channels": {
                "0": [
                  ["0x70", "0x71", "0x72"]
                ]
              }
            }
          ]
        }
      },
      "module_digits_width": 4,
      "module_digits_height": 4,
      "total_width": 12,
      "total_height": 8
    }
  }
}
```

### 直接接続の構成

```json
{
  "configurations": {
    "12x8-direct": {
      "name": "12x8 Direct",
      "buses": {
        "0": {
          "tca9548as": [
            {
              "address": null,
              "channels": {
                "-1": [
                  ["0x70", "0x71", "0x72"],
                  ["0x73", "0x74", "0x75"]
                ]
              }
            }
          ]
        }
      },
      "module_digits_width": 4,
      "module_digits_height": 4,
      "total_width": 12,
      "total_height": 8
    }
  }
}
```

### 48x8 行ベース構成 (rows 使用)

```json
{
  "configurations": {
    "48x8": {
      "name": "48x8 Expanded (4 channels with row-based layout)",
      "buses": {
        "1": {
          "tca9548as": [
            {
              "address": "0x77",
              "channels": {
                "0": [
                  ["0x70", "0x71", "0x72", "0x73", "0x74", "0x75"]
                ],
                "1": [
                  ["0x70", "0x71", "0x72", "0x73", "0x74", "0x75"]
                ],
                "2": [
                  ["0x70", "0x71", "0x72", "0x73", "0x74", "0x75"]
                ],
                "3": [
                  ["0x70", "0x71", "0x72", "0x73", "0x74", "0x75"]
                ]
              },
              "rows": {
                "0": {"channel": 0, "row_offset": 0, "col_offset": 0},
                "1": {"channel": 2, "row_offset": 0, "col_offset": 24},
                "2": {"channel": 1, "row_offset": 4, "col_offset": 0},
                "3": {"channel": 3, "row_offset": 4, "col_offset": 24}
              }
            }
          ]
        }
      },
      "module_digits_width": 4,
      "module_digits_height": 4,
      "total_width": 48,
      "total_height": 8
    }
  }
}
```

この構成では、4つの TCA9548A チャンネルが使用され、rows 設定により異なるチャンネルが同じ行に配置されます：
- 行0: チャンネル0 (左半分) と チャンネル2 (右半分) が同じ行に配置
- 行1: チャンネル1 (左半分) と チャンネル3 (右半分) が同じ行に配置

## 注意点

- アドレスは 16進数文字列で指定 (例: "0x70")
- 直接接続の場合、`address` を `null` に設定し、`channels` のキーを "-1" にする
- エミュレータの場合、`buses` を空オブジェクト `{}` に設定
- グリッドのサイズは `total_width` と `total_height` と一致するようにする
- TCA9548A のチャンネルは 0-7 の範囲を使用可能 (8チャンネル対応)

## 変更履歴

- 初期バージョン: 古い `tca9548a_address` と `channel_grids` 構造
- 現在のバージョン: `buses` 構造で複数の TCA9548A と Bus をサポート
- 48x8 対応: `rows` 構造で異なるチャンネルが同じ行に配置されるレイアウトをサポート</content>
<parameter name="filePath">/home/hijiri/7seg-panel/README.config.md