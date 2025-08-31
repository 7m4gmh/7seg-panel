#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include "config.h"
#include "config_loader.hpp"

// C++17の [[nodiscard]] 属性。戻り値を使わないと警告を出す。
[[nodiscard]]
bool parse_arguments(int argc, char* argv[], std::string& first_arg, std::string& config_name) {
    if (argc < 2) {
        return false;
    }
    first_arg = argv[1];
    if (argc > 2) {
        config_name = argv[2];
    } else {
        config_name = "24x4"; // デフォルト値
    }
    return true;
}

// 共通のmain処理テンプレート
// ラムダ式を使って、各プレイヤー固有のロジックを実行する
template<typename PlayerLogic>
int common_main_runner(const std::string& usage, int argc, char* argv[], PlayerLogic player_logic) {
    std::string first_arg;
    std::string config_name;

    if (!parse_arguments(argc, argv, first_arg, config_name)) {
        std::cerr << usage << std::endl;
        return 1;
    }

    try {
        DisplayConfig active_config = load_config_from_json(config_name);
        std::cout << "Using display configuration: " << active_config.name << std::endl;

        setup_signal_handlers();

        // 各プレイヤー固有のロジックをここで実行
        player_logic(first_arg, active_config);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\nProgram finished." << std::endl;
    return 0;
}