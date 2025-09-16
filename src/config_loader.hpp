#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <stdexcept>
#include "json.hpp" // nlohmann/json をインクルード
#include "config.h" // 既存のDisplayConfig構造体の定義をインクルード

// JSONをパースするためのヘルパー
using json = nlohmann::json;

// JSONから16進数文字列をintに変換するヘルパー関数
int hex_str_to_int(const std::string& hex_str) {
    return std::stoi(hex_str, nullptr, 16);
}

DisplayConfig load_config_from_json(const std::string& config_name, const std::string& filename = "config.json") {
    std::ifstream f(filename);
    if (!f.is_open()) {
        throw std::runtime_error("Could not open config file: " + filename);
    }
    json data = json::parse(f);

    if (!data["configurations"].contains(config_name)) {
        throw std::runtime_error("Configuration not found: " + config_name);
    }

    const auto& conf_json = data["configurations"][config_name];
    DisplayConfig config;

    config.name = conf_json["name"];
    if (conf_json.contains("type")) {
        config.type = conf_json["type"];
    } else {
        config.type = "physical";
    }

    if (conf_json.contains("buses")) {
        for (const auto& [bus_str, bus_json] : conf_json["buses"].items()) {
            int bus_id = std::stoi(bus_str);
            BusConfig bus_config;
            if (bus_json.contains("tca9548as")) {
                for (const auto& tca_json : bus_json["tca9548as"]) {
                    TCA9548AConfig tca;
                    if (tca_json.contains("address") && !tca_json["address"].is_null()) {
                        tca.address = hex_str_to_int(tca_json["address"]);
                    } else {
                        tca.address = -1;
                    }
                    if (tca_json.contains("channels")) {
                        for (const auto& [ch_str, grid_json] : tca_json["channels"].items()) {
                            int channel = std::stoi(ch_str);
                            std::vector<std::vector<int>> grid;
                            for (const auto& row_json : grid_json) {
                                std::vector<int> row;
                                for (const auto& addr_str : row_json) {
                                    row.push_back(hex_str_to_int(addr_str));
                                }
                                grid.push_back(row);
                            }
                            tca.channels[channel] = grid;
                        }
                    }
                    bus_config.tca9548as.push_back(tca);
                }
            }
            config.buses[bus_id] = bus_config;
        }
    }

    config.module_digits_width = conf_json["module_digits_width"];
    config.module_digits_height = conf_json["module_digits_height"];
    config.total_width = conf_json["total_width"];
    config.total_height = conf_json["total_height"];

    // C++版で使う物理寸法もJSONから読み込む
    CHAR_WIDTH_MM = data["char_width_mm"];
    CHAR_HEIGHT_MM = data["char_height_mm"];

    return config;
}