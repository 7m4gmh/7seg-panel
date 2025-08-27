// src/file_player.cpp
#include "common.h"
#include "playback.h" // ★★★ 共通の再生エンジンをインクルード
#include <iostream>
#include <string>
#include <atomic>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "使い方: " << argv[0] << " <動画ファイル> [config]" << std::endl;
        std::cerr << "  config: 24x4 (default) or 12x8" << std::endl;
        return 1;
    }
    std::string video_path = argv[1];
    std::string config_name = (argc > 2) ? argv[2] : "24x4";

    const DisplayConfig& active_config = (config_name == "12x8") ? CONFIG_12x8 : CONFIG_24x4;
    std::cout << "Using display configuration: " << active_config.name << std::endl;

    // シグナルハンドラ(Ctrl+C)を設定
    setup_signal_handlers();

    // ファイルプレイヤーにはHTTP経由の停止機能はないため、
    // このフラグは実質的に使われないが、関数のために渡す必要がある
    std::atomic<bool> stop_flag(false);

    // ★★★ 共通の再生関数を呼び出す ★★★
    play_video_stream(video_path, active_config, stop_flag);

    std::cout << "\nプログラムを終了します。" << std::endl;
    return 0;
}

