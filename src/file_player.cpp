// src/file_player.cpp
#include "common.h"
#include "playback.h" // 共通の再生エンジンをインクルード
#include <iostream>
#include <string>
#include <atomic>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>   // VideoCapture のために追加
#include <opencv2/imgcodecs.hpp> // imwrite のために追加

// ★★★ video.cpp に追加した関数のプロトタイプ宣言 ★★★
void debug_draw_sampling_points(const DisplayConfig& config, cv::Mat& output_frame);



int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "使い方: " << argv[0] << " <動画ファイル> [config]" << std::endl;
        std::cerr << "  config: 24x4 (default), 12x8, 48x4, 24x8" << std::endl;
        return 1;
    }
    std::string video_path = argv[1];
    std::string config_name = (argc > 2) ? argv[2] : "24x4";

    const DisplayConfig*  active_config_ptr = nullptr;
    if (config_name == "12x8") {
        active_config_ptr = &CONFIG_12x8_EXPANDED;
    } else if (config_name == "24x8") {
        active_config_ptr = &CONFIG_24x8_EXPANDED;
    } else if (config_name == "48x4") {
        active_config_ptr = &CONFIG_48x4_EXPANDED;
    } else {
        active_config_ptr = &CONFIG_24x4;
    }
    
    // ★★★ ここを修正 ★★★
    // active_config.name  ->  active_config_ptr->name
    std::cout << "Using display configuration: " << active_config_ptr->name << std::endl;

    /* 
   // ★★★ ここからデバッグコードを追加 ★★★
    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open video file." << std::endl;
        return 1;
    }
    cv::Mat first_frame;
    cap.read(first_frame);
    if (first_frame.empty()) {
        std::cerr << "Error: Could not read the first frame." << std::endl;
        return 1;
    }
    
    // サンプリング点をフレームに描画
    debug_draw_sampling_points(*active_config_ptr, first_frame);

    // 結果を画像ファイルとして保存
    std::string output_filename = "debug_sampling_" + config_name + ".png";
    cv::imwrite(output_filename, first_frame);
    std::cout << "Debug image saved as: " << output_filename << std::endl;
    
    // デバッグ目的のため、ここでプログラムを終了する
    return 0; 
    // ★★★ ここまでデバッグコード ★★★

*/

    setup_signal_handlers();

    std::atomic<bool> stop_flag(false);

    play_video_stream(video_path, *active_config_ptr, stop_flag);

    std::cout << "\nプログラムを終了します。" << std::endl;
    return 0;
}