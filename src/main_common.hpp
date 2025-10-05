#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include "config.h"
#include "config_loader.hpp"
#include <map>
#include <utility>

extern std::map<std::pair<int, int>, int> g_error_counts;


#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include "config.h"
#include "config_loader.hpp"
#include <map>
#include <utility>
#include "playback.h" // ScalingModeのため

extern std::map<std::pair<int, int>, int> g_error_counts;


// C++17の [[nodiscard]] 属性。戻り値を使わないと警告を出す。
[[nodiscard]]
bool parse_arguments(int argc, char* argv[], std::string& first_arg, std::string& config_name, 
                    ScalingMode& scaling_mode, int& min_threshold, int& max_threshold, bool& debug) {
    if (argc < 2) {
        return false;
    }
    first_arg = argv[1];
    config_name = "24x4"; // デフォルト値
    scaling_mode = ScalingMode::FIT; // デフォルト値
    min_threshold = 64; // デフォルト値
    max_threshold = 255; // デフォルト値

    debug = false;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            debug = true;
            continue;
        }
        if (arg == "--stretch" || arg == "-s") {
            scaling_mode = ScalingMode::STRETCH;
        } else if (arg == "--crop" || arg == "-c") {
            scaling_mode = ScalingMode::CROP;
        } else if (arg == "--fit" || arg == "-f") {
            scaling_mode = ScalingMode::FIT;
        } else if (arg == "--threshold" || arg == "-t") {
            if (i + 2 < argc) {
                try {
                    min_threshold = std::stoi(argv[i + 1]);
                    max_threshold = std::stoi(argv[i + 2]);
                    i += 2; // 2つの引数を消費
                } catch (const std::exception&) {
                    std::cerr << "Invalid threshold values. Expected integers." << std::endl;
                    return false;
                }
            } else {
                std::cerr << "Missing threshold values after " << arg << std::endl;
                return false;
            }
        } else {
            // 設定名として扱う
            config_name = arg;
        }
    }
    return true;
}

// 共通のmain処理テンプレート
// ラムダ式を使って、各プレイヤー固有のロジックを実行する
template<typename PlayerLogic>
int common_main_runner(const std::string& usage, int argc, char* argv[], PlayerLogic player_logic) {
    std::string first_arg;
    std::string config_name;
    ScalingMode scaling_mode;
    int min_threshold, max_threshold;
    bool debug = false;

    if (!parse_arguments(argc, argv, first_arg, config_name, scaling_mode, min_threshold, max_threshold, debug)) {
        std::cerr << usage << std::endl;
        return 1;
    }

    try {
        DisplayConfig active_config = load_config_from_json(config_name);
        std::cout << "Using display configuration: " << active_config.name << std::endl;
        std::string mode_str;
        if (scaling_mode == ScalingMode::CROP) mode_str = "CROP";
        else if (scaling_mode == ScalingMode::STRETCH) mode_str = "STRETCH";
        else if (scaling_mode == ScalingMode::FIT) mode_str = "FIT";
        std::cout << "Scaling mode: " << mode_str << std::endl;
        std::cout << "Threshold: " << min_threshold << " - " << max_threshold << std::endl;

        setup_signal_handlers();

    // 各プレイヤー固有のロジックをここで実行
    player_logic(first_arg, active_config, scaling_mode, min_threshold, max_threshold, debug);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\nProgram finished." << std::endl;
    return 0;
}