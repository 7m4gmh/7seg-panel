#include "common.h"
#include "led.h"
#include "playback.h"
#include "main_common.hpp" 
#include "video.h"
#include "file_audio_gst.h"
#include "audio.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

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

            auto play_fileplayer = [&](void) -> int {
                // 非同期I2Cライターを使った再生（file-player専用）
                std::atomic<bool> writer_stop(false);
                std::mutex q_mtx;
                std::condition_variable q_cv;
                std::deque<std::vector<uint8_t>> q;

                // I2Cオープンと初期化はメインスレッドで行い、ライタースレッドは同じfdを使う
                int i2c_fd = open_i2c_auto(config);
                if (i2c_fd < 0) {
                    std::cerr << "I2C communication failed: No I2C devices found or access denied." << std::endl;
                    return -1;
                }
                if (!initialize_displays(i2c_fd, config)) {
                    std::cerr << "Failed to initialize display modules." << std::endl;
                    close(i2c_fd);
                    return -1;
                }

                // ライタースレッド
                std::thread writer([&]{
                    while (!writer_stop) {
                        std::vector<uint8_t> frame;
                        {
                            std::unique_lock<std::mutex> lk(q_mtx);
                            if (q.empty()) {
                                q_cv.wait_for(lk, std::chrono::milliseconds(5));
                                if (q.empty()) continue;
                            }
                            // 最新フレームだけ残す（遅延削減）
                            while (q.size() > 1) q.pop_front();
                            frame = std::move(q.front());
                            q.pop_front();
                        }

                        if (frame.empty()) continue;
                        I2CErrorInfo err;
                        // 非同期でも既存の更新関数を使う（モジュール単位で失敗しても戻る）
                        if (!update_flexible_display(i2c_fd, config, frame, err)) {
                            std::cerr << "[file-player writer] I2C write failed (non-blocking): attempting recovery" << std::endl;
                            // 復旧は試すが、失敗してもループ継続（メインはブロックされない）
                            attempt_i2c_recovery(i2c_fd, config);
                        }
                    }
                });

                // --- ビデオ/オーディオ開始とフレーム生成（主ループ） ---
                cv::VideoCapture cap;
                if (video_path == "-") {
                    const std::string gst_pipeline = 
                         "fdsrc ! decodebin name=d "
                        "d. ! queue ! videoconvert ! appsink "
                        "d. ! queue ! audioconvert ! audioresample ! autoaudiosink";
                    cap.open(gst_pipeline, cv::CAP_GSTREAMER);
                } else {
                    cap.open(video_path, cv::CAP_FFMPEG);
                }
                if (!cap.isOpened()) {
                    std::cerr << "動画ソースを開けません: " << video_path << std::endl;
                    writer_stop = true; q_cv.notify_all(); writer.join(); close(i2c_fd); return -1;
                }

                bool use_ffplay = false;
                if (const char* e = std::getenv("FILE_AUDIO_USE_FFPLAY")) {
                    if (std::string(e) == "1" || std::string(e) == "true") use_ffplay = true;
                }
                if (video_path != "-") {
                    if (use_ffplay) {
                        std::string command = "ffplay -nodisp -autoexit \"" + video_path + "\" > /dev/null 2>&1 &";
                        system(command.c_str());
                    } else {
                        bool inited = audio_init(SAMPLE_RATE, CHANNELS);
                        bool ok = inited && file_audio_start(video_path);
                        if (!ok) {
                            if (inited) audio_cleanup();
                            std::string command = "ffplay -nodisp -autoexit \"" + video_path + "\" > /dev/null 2>&1 &";
                            system(command.c_str());
                            use_ffplay = true;
                        }
                    }
                }

                double fps = cap.get(cv::CAP_PROP_FPS);
                if (fps <= 0) fps = 30.0;
                auto frame_duration = std::chrono::microseconds(static_cast<long long>(1000000.0 / fps));

                auto next_frame_time = std::chrono::steady_clock::now();
                cv::Mat frame;
                while (!g_should_exit && !stop_flag && cap.read(frame)) {
                    cv::Mat gray;
                    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
                    cv::Mat bw;
                    cv::threshold(gray, bw, min_threshold, max_threshold, cv::THRESH_BINARY);
                    std::vector<uint8_t> grid;
                    frame_to_grid(bw, config, grid);

                    // enqueue latest frame (非ブロッキング)
                    {
                        std::lock_guard<std::mutex> lk(q_mtx);
                        q.push_back(grid);
                        if (q.size() > 4) q.pop_front();
                    }
                    q_cv.notify_one();

                    next_frame_time += frame_duration;
                    std::this_thread::sleep_until(next_frame_time);
                }

                // 停止処理
                writer_stop = true; q_cv.notify_all();
                if (writer.joinable()) writer.join();
                if (video_path != "-") {
                    if (use_ffplay) system("killall ffplay > /dev/null 2>&1"); else { file_audio_stop(); audio_cleanup(); }
                }
                cap.release();
                close(i2c_fd);
                return 0;
            };

            if (loop) {
                std::cerr << "[file_player] loop enabled" << std::endl;
                // 指定があれば動画終了後に繰り返す（stop_flag が立てられたら終了）
                int loop_count = 0;
                while (!stop_flag && !g_should_exit) {
                    loop_count++;
                    if (debug) std::cerr << "[file_player] starting loop iteration " << loop_count << std::endl;
                    int rc = play_fileplayer();
                    if (debug) std::cerr << "[file_player] play_fileplayer returned rc=" << rc << std::endl;
                    if (stop_flag) break;
                    if (g_should_exit) break;
                    // 再開前に少し待つ（無限ループ防止）
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    (void)rc; // rc は将来的な利用のために残す
                }
                std::cerr << "[file_player] loop exiting after " << loop_count << " iterations" << std::endl;
            } else {
                (void)play_fileplayer();
            }
            
            // ここでの待ちは不要。playback ループは g_should_exit を見て終了する。

            // 再生終了時に物理パネルを消灯する（エミュレータでは不要）
            if (config.type != "emulator") {
                if (!clear_all_displays(config)) {
                    std::cerr << "Warning: failed to clear displays on exit." << std::endl;
                } else {
                    std::cerr << "Displays cleared on exit." << std::endl;
                }
            }
        }
    );
}