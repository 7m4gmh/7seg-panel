// player.cpp 
#include "common.h"
#include "led.h"
#include "video.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>

extern volatile sig_atomic_t g_should_exit;


void frame_to_grid(const cv::Mat& bw_frame, std::vector<uint8_t>& grid);
void update_display(int i2c_fd, const std::vector<uint8_t>& grid, const std::vector<int>& addrs);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "使い方: " << argv[0] << " <動画ファイル>" << std::endl;
        return 1;
    }
    std::string video_path = argv[1];

    setup_signal_handlers();

    int i2c_fd = open("/dev/i2c-0", O_RDWR);
    if (i2c_fd < 0) {
        perror("open /dev/i2c-0 に失敗");
        return 1;
    }

    if (!initialize_displays(i2c_fd, MODULE_ADDRESSES)) {
        std::cerr << "Failed to initialize display modules." << std::endl;
        close(i2c_fd);
        return 1;
    }

    cv::VideoCapture cap(video_path, cv::CAP_FFMPEG);
    if (!cap.isOpened()) {
        std::cerr << "動画ファイルを開けません: " << video_path << std::endl;
        close(i2c_fd);
        return 1;
    }

    std::string command = "ffplay -nodisp -autoexit \"" + video_path + "\" > /dev/null 2>&1 &";
    system(command.c_str());

    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;
    auto frame_duration = std::chrono::microseconds(static_cast<long long>(1000000.0 / fps));
    std::cout << "再生中: " << video_path << " (" << fps << " FPS, 解像度 " << W << "x" << H << ")" << std::endl;

    auto next_frame_time = std::chrono::steady_clock::now();
    cv::Mat frame;

    while (!g_should_exit && cap.read(frame)) {
        cv::Mat cropped_frame;
        float source_aspect = (float)frame.cols / frame.rows;
        const float target_aspect = (float)W / H;

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

        cv::Mat resized_frame;
        cv::resize(cropped_frame, resized_frame, cv::Size(W, H));

        cv::Mat gray_frame, bw_frame;
        cv::cvtColor(resized_frame, gray_frame, cv::COLOR_BGR2GRAY);
        cv::threshold(gray_frame, bw_frame, 128, 255, cv::THRESH_BINARY);

        // ★★★★★★★★★★★★★★★★★★★★★★★★★★★★
        // ★★★  ここが修正点です ★★★
        // ★★★★★★★★★★★★★★★★★★★★★★★★★★★★
        std::vector<uint8_t> grid(TOTAL, 0);

        // デバッグでコメントアウトした2行を元に戻します
        frame_to_grid(bw_frame, grid);
        update_display(i2c_fd, grid, MODULE_ADDRESSES);

        next_frame_time += frame_duration;
        std::this_thread::sleep_until(next_frame_time);
    }

    system("killall ffplay > /dev/null 2>&1");
    
    std::cout << "\n終了します..." << std::endl;
    cap.release();
    close(i2c_fd);

    return 0;
}

