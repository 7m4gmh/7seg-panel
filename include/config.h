// src/config.h
#pragma once

#include <vector>
#include <string>
#include <numeric>
#include <map>
#include <algorithm> // for std::sort, std::unique

extern double CHAR_WIDTH_MM;
extern double CHAR_HEIGHT_MM;

// TCA9548A の構成を定義する構造体
struct TCA9548AConfig {
    int address; // TCA9548A のアドレス (-1 で直接接続)
    std::map<int, std::vector<std::vector<int>>> channels; // channel -> slave address grid
    std::map<int, std::tuple<int, int, int>> rows; // row -> (channel, row_offset, col_offset)
};

// Bus の構成を定義する構造体
struct BusConfig {
    std::vector<TCA9548AConfig> tca9548as;
};

// パネルの物理構成を定義する構造体
struct DisplayConfig {
    std::string name;
    std::string type; // "physical" or "emulator"
    std::map<int, BusConfig> buses;
    int module_digits_width;
    int module_digits_height;
    int total_width;
    int total_height;

    int total_digits() const { return total_width * total_height; }

    std::vector<int> all_addresses() const {
        std::vector<int> addrs;
        for (const auto& [bus_id, bus_config] : buses) {
            for (const auto& tca : bus_config.tca9548as) {
                for (const auto& [channel, grid] : tca.channels) {
                    for (const auto& row : grid) {
                        addrs.insert(addrs.end(), row.begin(), row.end());
                    }
                }
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

