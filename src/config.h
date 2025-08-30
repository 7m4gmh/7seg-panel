// src/config.h
#pragma once

#include <vector>
#include <string>
#include <numeric>
#include <map>
#include <algorithm> // for std::sort, std::unique

// パネルの物理構成を定義する構造体
struct DisplayConfig {
    std::string name;
    int tca9548a_address; // エクスパンダのアドレス (使わない場合は-1)
    std::map<int, std::vector<std::vector<int>>> channel_grids;
    int module_digits_width;
    int module_digits_height;
    int total_width;
    int total_height;

    int total_digits() const { return total_width * total_height; }

    std::vector<int> all_addresses() const {
        std::vector<int> addrs;
        for (auto const& [channel, grid] : channel_grids) {
            for (const auto& row : grid) {
                addrs.insert(addrs.end(), row.begin(), row.end());
            }
        }
        std::sort(addrs.begin(), addrs.end());
        addrs.erase(std::unique(addrs.begin(), addrs.end()), addrs.end());
        return addrs;
    }
};


// LEDキャラクター1つの物理的な寸法 (mm)
const double CHAR_WIDTH_MM = 12.7;
const double CHAR_HEIGHT_MM = 19.2;

// 元の構成: 24桁 x 4行
const DisplayConfig CONFIG_24x4 = {
    "24x4",
    0x77,
    { // module_grid
        {0, {
            {0x70, 0x71, 0x72, 0x73, 0x74, 0x75}
        }}
    },
    4,  // module_digits_width
    4,  // module_digits_height
    24, // total_width
    4   // total_height
};

/*
// 拡張構成の例: 12桁 x 8行
const DisplayConfig CONFIG_12x8 = {
    "12x8",
    0x77, // TCA9548Aを使用しない場合は -1
    {
        {0, {
            {0x70, 0x71, 0x72},
            {0x73, 0x74, 0x75}
        }},
    },
    4,  // module_digits_width
    4,  // module_digits_height
    12, // total_width
    8   // total_height
};
*/

// 12x8 の拡張構成
const DisplayConfig CONFIG_12x8_EXPANDED = {
    "12x8 Expanded (2x 12x4 via TCA9548A)",
    0x77, // ★★★ TCA9548Aのアドレスを 0x77 に設定 ★★★
    {
        // Channel 0 に最初の12x4パネル群 (3モジュール)
        {0, {
            {0x70, 0x71, 0x72}
        }},
        // Channel 1 に2番目の12x4パネル群 (3モジュール)
        {1, {
            {0x70, 0x71, 0x72}
        }}
    },
    4,  // module_digits_width
    4,  // module_digits_height
    12, // total_width
    8   // total_height
};

// 48x4 (24x4を2セット) の拡張構成
const DisplayConfig CONFIG_48x4_EXPANDED = {
    "48x4 Expanded (2x 24x4 via TCA9548A)",
    0x77, // ★★★ TCA9548Aのアドレスを 0x77 に設定 ★★★
    {
        // Channel 0 に最初の24x4パネル群 (6モジュール)
        {0, {
            {0x70, 0x71, 0x72, 0x73, 0x74, 0x75}
        }},
        // Channel 1 に2番目の24x4パネル群 (6モジュール)
        {1, {
            {0x70, 0x71, 0x72, 0x73, 0x74, 0x75}
        }}
    },
    4,  // module_digits_width
    4,  // module_digits_height
    48, // total_width
    4   // total_height
};

// 24x8 (24x4を2セット) の拡張構成
const DisplayConfig CONFIG_24x8_EXPANDED = {
    "24x8 Expanded (2x 24x4 via TCA9548A)",
    0x77, // ★★★ TCA9548Aのアドレスを 0x77 に設定 ★★★
    {
        // Channel 0 に最初の24x4パネル群 (6モジュール)
        {0, {
            {0x70, 0x71, 0x72, 0x73, 0x74, 0x75}
        }},
        // Channel 1 に2番目の24x4パネル群 (6モジュール)
        {1, {
            {0x70, 0x71, 0x72, 0x73, 0x74, 0x75}
        }}
    },
    4,  // module_digits_width
    4,  // module_digits_height
    24, // total_width
    8   // total_height
};

