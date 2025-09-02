// src/playback.cpp
#include "playback.h"
#include "common.h"
#include "led.h"
#include "video.h" // ★★★ 'frame_to_grid' のために必要 ★★★
#include <opencv2/opencv.hpp> // ★★★ 'cv::' 関連 ★★★
#include <iostream>
#include <chrono>             // ★★★ 'std::chrono' のために必要 ★★★
#include <thread>             // ★★★ 'std::this_thread' のために必要 ★★★
#include <map>    // std::map のために必要
#include <utility> // std::pair のために必要
#include <csignal> // signal のために必要
#include <unistd.h>
#include <fcntl.h>
#include <atomic>             // std::atomic のために念のため

std::atomic<bool>* g_current_stop_flag = nullptr;
//std::map<std::pair<int, int>, int> g_error_counts;

/*
void handle_signal(int signal) {
    if (signal == SIGINT) {
        // グローバルポインタが設定されていれば、
        // それが指す先のstop_flagをtrueにする
        if (g_current_stop_flag) {
            *g_current_stop_flag = true;
        }
    }
}
*/

// 共通の動画再生ロジック
int play_video_stream(const std::string& video_path, const DisplayConfig& config, std::atomic<bool>& stop_flag) {
    g_current_stop_flag = &stop_flag;
    stop_flag = false;
    
    int i2c_fd = open("/dev/i2c-0", O_RDWR);
    if (i2c_fd < 0) {
        perror("open /dev/i2c-0 に失敗");
        return -1;
    }

    if (!initialize_displays(i2c_fd, config)) {
        std::cerr << "Failed to initialize display modules." << std::endl;
        close(i2c_fd);
        return -1;
    }

    cv::VideoCapture cap;
    if (video_path == "-") {
        std::cout << "標準入力からビデオストリームを読み込みます..." << std::endl;
        // GStreamerパイプラインを使って標準入力(fd=0)から読み込む
        //const std::string gst_pipeline = "fdsrc ! matroskademux ! videoconvert ! appsink";
        //const std::string gst_pipeline = "fdsrc ! tsdemux ! videoconvert ! appsink";
        // tsdemux と videoconvert の間に、avdec_h264 を追加
        //const std::string gst_pipeline = "fdsrc ! tsdemux ! avdec_h264 ! videoconvert ! appsink";
        //const std::string gst_pipeline = "fdsrc ! decodebin ! videoconvert ! appsink";
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
        close(i2c_fd);
        return -1;
    }

    // ffplayで音声のみ再生
    std::string command = "ffplay -nodisp -autoexit \"" + video_path + "\" > /dev/null 2>&1 &";
    system(command.c_str());

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;
    auto frame_duration = std::chrono::microseconds(static_cast<long long>(1000000.0 / fps));
    std::cout << "再生開始: " << video_path << " (" << fps << " FPS)" << std::endl;

    auto next_frame_time = std::chrono::steady_clock::now();
    cv::Mat frame;

    // g_should_exit (Ctrl+C) と stop_flag (ロジックによる停止) の両方をチェック
    while (!g_should_exit && !stop_flag && cap.read(frame)) {
        cv::Mat cropped_frame;
        float source_aspect = static_cast<float>(frame.cols) / frame.rows;
        const float target_aspect = static_cast<float>(config.total_width) / config.total_height;

        if (source_aspect > target_aspect) {
            int new_width = static_cast<int>(frame.rows * target_aspect);
            int x = (frame.cols - new_width) / 2;
            cv::Rect crop_region(x, 0, new_width, frame.rows);
            cropped_frame = frame(crop_region);
        } else {
            int new_height = static_cast<int>(frame.cols / target_aspect);
            int y = (frame.rows - new_height) / 2;
            cv::Rect crop_region(0, y, frame.cols, new_height);
            cropped_frame = frame(crop_region);
        }

        cv::Mat resized_frame, gray_frame, bw_frame;
        cv::resize(cropped_frame, resized_frame, cv::Size(W, H));
        cv::cvtColor(resized_frame, gray_frame, cv::COLOR_BGR2GRAY);
        cv::threshold(gray_frame, bw_frame, 128, 255, cv::THRESH_BINARY);

        std::vector<uint8_t> grid;
        frame_to_grid(bw_frame, config, grid);
   
        I2CErrorInfo error_info;

        if (!update_flexible_display(i2c_fd, config, grid, error_info)) {
               if (error_info.error_occurred) {
                //g_error_counts[{error_info.channel, error_info.address}]++;
            }
            if (!attempt_i2c_recovery(i2c_fd, config)) {
            // 全ての復旧に失敗した場合、長めに待つ
            std::cerr << "Recovery failed. Pausing before next attempt..." << std::endl;
            sleep(2);    
            }   
        }
        next_frame_time += frame_duration;
        std::this_thread::sleep_until(next_frame_time);
    }

    system("killall ffplay > /dev/null 2>&1");
    cap.release();
    close(i2c_fd);

    if (stop_flag) {
        std::cout << "再生中止: " << video_path << std::endl;
    } else {
        std::cout << "再生終了: " << video_path << std::endl;
    }
    //handle_signal(SIGINT);
    g_current_stop_flag = nullptr;
    return 0;
}

