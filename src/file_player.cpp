#include "common.h"
#include "playback.h"
#include "main_common.hpp" 
#include <thread>   

int main(int argc, char* argv[]) {
    const std::string usage = 
        "Usage: " + std::string(argv[0]) + " <video_file> [config_name]\n"
        "  config_name: 24x4 (default), 12x8, etc. from config.json";

    // common_main_runner を呼び出し、ファイル再生ロジックをラムダ式で渡す
    return common_main_runner(usage, argc, argv, 
        [](const std::string& video_path, const DisplayConfig& config) {
            std::atomic<bool> stop_flag(false);
            // g_should_exit は共通シグナルハンドラで更新される
            std::thread watch_dog([&stop_flag] {
                while(!g_should_exit) {
                     std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                stop_flag = true;
            });
            
            if (config.type == "emulator") {
                play_video_stream_emulator(video_path, config, stop_flag);
            } else {
                play_video_stream(video_path, config, stop_flag);
            }
            
            if(watch_dog.joinable()) {
                watch_dog.join();
            }
        }
    );
}