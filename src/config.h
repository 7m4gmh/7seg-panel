// src/config.h
#pragma once

#include <vector>
#include <string>
#include <numeric>

// パネルの物理構成を定義する構造体
struct DisplayConfig {
    std::string name; // 設定の名前 (例: "24x4 Horizontal")

    // モジュールの物理的な配置 (行x列) をI2Cアドレスで指定
    // 例: {{0x70, 0x71}, {0x72, 0x73}} は2x2のグリッド配置を示す
    std::vector<std::vector<int>> module_grid;

    int module_digits_width;  // 1モジュールあたりの表示桁数 (横)
    int module_digits_height; // 1モジュールあたりの表示桁数 (縦)
    int total_width;          // パネル全体の表示桁数 (横)
    int total_height;         // パネル全体の表示桁数 (縦)

    // --- ヘルパー関数 ---

    // パネル全体の合計桁数を計算して返す
    int total_digits() const {
        return total_width * total_height;
    }

    // module_gridから全モジュールのI2Cアドレスを1次元のリストとして返す
    std::vector<int> all_addresses() const {
        std::vector<int> addrs;
        for (const auto& row : module_grid) {
            addrs.insert(addrs.end(), row.begin(), row.end());
        }
        return addrs;
    }
};

// --- ここで具体的な設定を定義 ---

// 元の構成: 24桁 x 4行
const DisplayConfig CONFIG_24x4 = {
    "24x4 Horizontal",
    { // module_grid
        {0x70, 0x71, 0x72, 0x73, 0x74, 0x75}
    },
    4,  // module_digits_width
    4,  // module_digits_height
    24, // total_width
    4   // total_height
};

// 拡張構成の例: 12桁 x 8行
const DisplayConfig CONFIG_12x8 = {
    "12x8 Grid",
    { // module_grid
        {0x70, 0x71, 0x72},
        {0x73, 0x74, 0x75}
    },
    4,  // module_digits_width
    4,  // module_digits_height
    12, // total_width
    8   // total_height
};


