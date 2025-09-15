// src/config.h
#pragma once

#include <vector>
#include <string>
#include <numeric>
#include <map>
#include <algorithm> // for std::sort, std::unique

extern double CHAR_WIDTH_MM;
extern double CHAR_HEIGHT_MM;

// パネルの物理構成を定義する構造体
struct DisplayConfig {
    std::string name;
    std::string type; // "physical" or "emulator"
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
extern double CHAR_WIDTH_MM;
extern double CHAR_HEIGHT_MM;

