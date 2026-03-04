**Module Mapping (モジュールマッピング)**

簡潔: パネルごとの物理的な桁・セグメント配置を `config.json` で定義できます。

**サマリ**:
- **目的**: ハードウェアごとに桁順や配線が違う場合に、プログラムを修正せず設定だけで対応する。
- **形式**: `module_column_reverse`（真偽値）または `module_index_map`（明示的なインデックス配列）を使用します。

**設定場所**:
- ファイル: [config.json](config.json)

**優先順位（フォールバック）**:
1. `module_index_map` が存在すればそれを使用（論理 -> 物理インデックスの完全マップ）
2. `module_column_reverse` が true なら列（横方向）を反転して適用
3. 上記が無ければ既定の行優先（row-major）順序を使用

**キーのルール**:
- モジュールのアドレスは文字列で指定します（例: `"0x70"`）。
- `module_index_map` の配列長は `module_digits_width * module_digits_height` と一致させてください。

**例: 列反転 (簡易)**
```
"module_column_reverse": {
  "0x70": true,
  "0x71": true
}
```

この場合、該当モジュール内の列順が左右反転されます（モジュールが左右逆に配線されているときに有効）。

**例: 明示的なインデックスマップ（推奨・柔軟）**
```
"module_index_map": {
  "0x70": [6,7,4,5,2,3,0,1,14,15,12,13,10,11,8,9],
  "0x71": [6,7,4,5,2,3,0,1,14,15,12,13,10,11,8,9]
}
```
配列インデックスの意味: 配列の位置 `i` が「論理インデックス」（左上から行優先で i=0..N-1）で、配列の値 `module_index_map[ i ]` がその論理位置に書き込むべき物理インデックス（モジュール内部の配線インデックス）です。

具体例: `module_digits_width=8, module_digits_height=2` の場合、論理インデックスは
```
 0 1 2 3 4 5 6 7
 8 9 A B C D E F
```
で表され、上記マップは各論理位置がどの物理位置に当たるかを指定します。

**コード側の振る舞い（参照用）**
- JSON 読み込み: [src/config_loader.hpp](src/config_loader.hpp) で `module_column_reverse` / `module_index_map` を読み込みます。
- マッピング適用: [src/led.cpp](src/led.cpp) の表示更新処理が、`module_index_map` を最優先で使い、無ければ `module_column_reverse` を参照します。

**テスト方法**
- 既存のテスト設定を使う:
  - `8x2-direct-map` / `8x4-direct-rev` はサンプルです。
- ビルドと起動:
```
make core
sudo ./bin/*/7seg-udp-player 5000 8x4-direct-rev
```
- マッピング確認用スクリプト:
  - `scripts/test_mapping.py --name <config> --live` で各文字位置を順に点灯できます（`smbus2` が必要）。

**実践的なアドバイス**
- まず `module_index_map` を作って小さな範囲で試すと確実です。
- モジュールごとに異なる配線がある場合は、各アドレスに対して個別に `module_index_map` を指定します。

**参考ファイル**:
- JSON 読み込み: [src/config_loader.hpp](src/config_loader.hpp)
- マッピング適用: [src/led.cpp](src/led.cpp)
- 例設定: [config.json](config.json)

---

必要があればこのドキュメントに図や追加例（4x4 の例、8x2 の物理配線図）を追加します。どの例を足しましょうか？
