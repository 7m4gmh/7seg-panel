#include "common.h"
#include "playback.h"
#include "main_common.hpp" 
#include <thread>   

int main(int argc, char* argv[]) {
    const std::string usage = 
        "Usage: " + std::string(argv[0]) + " <video_file> [config_name] [options]\n"
        "  config_name: 24x4 (default), 12x8, etc. from config.json\n"
        "  options:\n"
        "    --crop, -c: Crop to fit aspect ratio\n"
        "    --stretch, -s: Stretch to fill display\n"
        "    --fit, -f: Fit entire video within display (may add padding) (default)\n"
        "    --threshold min max, -t min max: Set binarization threshold (default: 64 255)\n"
        "    --loop, -l: Repeat video playback (file-player only)";

    // common_main_runner を呼び出し、ファイル再生ロジックをラムダ式で渡す
    return common_main_runner(usage, argc, argv, 
        [](const std::string& video_path, const DisplayConfig& config, ScalingMode scaling_mode, int min_threshold, int max_threshold, bool debug, bool loop) {
            std::atomic<bool> stop_flag(false);
            // g_should_exit は共通シグナルハンドラで更新される
            // watch_dog スレッドは不要（再生ループ側で g_should_exit を参照するため）
            
            // 再生関数をラップ
            auto play_once = [&](void) -> int {
                if (config.type == "emulator") {
                    return play_video_stream_emulator(video_path, config, stop_flag, scaling_mode, min_threshold, max_threshold, debug);
                } else {
                    return play_video_stream(video_path, config, stop_flag, scaling_mode, min_threshold, max_threshold, debug);
                }
            };

            if (loop) {
                std::cerr << "[file_player] loop enabled" << std::endl;
                // 指定があれば動画終了後に繰り返す（stop_flag が立てられたら終了）
                int loop_count = 0;
                while (!stop_flag) {
                    loop_count++;
                    if (debug) std::cerr << "[file_player] starting loop iteration " << loop_count << std::endl;
                    int rc = play_once();
                    if (debug) std::cerr << "[file_player] play_once returned rc=" << rc << std::endl;
                    if (stop_flag) break;
                    // 再開前に少し待つ（無限ループ防止）
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    (void)rc; // rc は将来的な利用のために残す
                }
                std::cerr << "[file_player] loop exiting after " << loop_count << " iterations" << std::endl;
            } else {
                (void)play_once();
            }
            
            // ここでの待ちは不要。playback ループは g_should_exit を見て終了する。
        }
    );
}