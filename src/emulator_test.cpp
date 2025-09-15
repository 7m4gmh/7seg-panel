// emulator_test.cpp
// エミュレータ単体テスト用
#include "emulator_display.h"
#include <thread>
#include <chrono>
#include <vector>
#include <cstdint>
#include <opencv2/opencv.hpp>  // ←これを追加
#include <fstream>
#include <iostream>
#include "json.hpp"

int main(int argc, char* argv[]) {
    std::string config_name = "emulator-12x8"; // デフォルト
    bool debug = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug") {
            debug = true;
        } else if (config_name == "emulator-12x8") { // 最初の非--debug引数を構成名とする
            config_name = arg;
        }
    }

    // デバッグモード設定
    debug_mode = debug;

    // JSONから構成を読み込み
    std::ifstream f("config.json");
    if (!f.is_open()) {
        std::cerr << "Failed to open config.json" << std::endl;
        return 1;
    }
    nlohmann::json config;
    f >> config;
    if (config["configurations"].find(config_name) == config["configurations"].end()) {
        std::cerr << "Configuration " << config_name << " not found" << std::endl;
        return 1;
    }
    auto& conf = config["configurations"][config_name];
    int total_width = conf["total_width"];
    int total_height = conf["total_height"];

    IDisplayOutput* display = create_emulator_display(config_name);
    if (!display) {
        std::cerr << "Failed to create emulator display" << std::endl;
        return 1;
    }

    std::vector<uint8_t> patterns = {
        0b00111111, // 0
        0b00000110, // 1
        0b01011011, // 2
        0b01001111, // 3
        0b01100110, // 4
        0b01101101, // 5
        0b01111101, // 6
        0b00000111, // 7
        0b01111111, // 8
        0b01101111  // 9
    };
    for(int i=0; i<10; ++i) {
        std::vector<uint8_t> grid;
        for(int r=0; r<total_height; ++r) {
            for(int c=0; c<total_width; ++c) {
                // 行ごとに異なるパターン（0,1,2,3...）
                grid.push_back(patterns[r % patterns.size()]);
            }
        }
        display->update(grid);
        cv::waitKey(800); // 800ms表示
    }
    cv::waitKey(0); // 最後の画面でキー入力待ち
    delete display;
    return 0;
}
