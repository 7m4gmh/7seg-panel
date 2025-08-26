#include "video.h"
#include "common.h"
#include "led.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <map>

const std::map<int, cv::Point> SEGMENT_SAMPLING_MAP_LANDSCAPE = {
    {0, {1, 0}}, {1, {3, 1}}, {2, {3, 3}}, {3, {1, 4}},
    {4, {0, 3}}, {5, {0, 1}}, {6, {1, 2}}, {7, {4, 4}},
};
const std::map<int, cv::Point> SEGMENT_SAMPLING_MAP_PORTRAIT = {
    {1, {3, 1}}, {2, {3, 3}}, {4, {0, 3}}, {5, {0, 1}}, {0, {1, 0}},
    {3, {1, 4}}, {6, {1, 2}}, {7, {4, 4}},
};

std::vector<uint8_t> frame_to_grid_data(const cv::Mat& bw_frame, DisplayOrientation orientation) {
    std::vector<uint8_t> grid(TOTAL_DIGITS, 0);
    const auto& current_sampling_map = (orientation == DisplayOrientation::LANDSCAPE)
                                     ? SEGMENT_SAMPLING_MAP_LANDSCAPE
                                     : SEGMENT_SAMPLING_MAP_PORTRAIT;
    if (orientation == DisplayOrientation::LANDSCAPE) {
        for (int i = 0; i < TOTAL_DIGITS; ++i) {
            int row = i / DISPLAYS_ACROSS, col = i % DISPLAYS_ACROSS;
            int frame_x_start = col * CELL_WIDTH, frame_y_start = row * CELL_HEIGHT;
            uint8_t segment_byte = 0;
            for (const auto& pair : current_sampling_map) {
                int bit = pair.first;
                cv::Point offset = pair.second;
                int px = frame_x_start + offset.x, py = frame_y_start + offset.y;
                if (py < bw_frame.rows && px < bw_frame.cols && bw_frame.at<uchar>(py, px) > 0)
                    segment_byte |= (1 << bit);
            }
            grid[i] = segment_byte;
        }
    } else {
        for (int i = 0; i < TOTAL_DIGITS; ++i) {
            int row = i / DISPLAYS_ACROSS, col = i % DISPLAYS_ACROSS;
            int frame_x_start = row * CELL_WIDTH, frame_y_start = col * CELL_HEIGHT;
            uint8_t segment_byte = 0;
            for (const auto& pair : current_sampling_map) {
                int bit = pair.first;
                cv::Point offset = pair.second;
                int px = frame_x_start + offset.x, py = frame_y_start + offset.y;
                if (py < bw_frame.rows && px < bw_frame.cols && bw_frame.at<uchar>(py, px) > 0)
                    segment_byte |= (1 << bit);
            }
            grid[i] = segment_byte;
        }
    }
    return grid;
}

int play_video(const std::string& video_path, DisplayOrientation orientation) {
    g_stop_current_video = false;
    int i2c_fd = open("/dev/i2c-0", O_RDWR);
    if (i2c_fd < 0) { perror("open /dev/i2c-0 に失敗"); return 1; }
    if (!initialize_displays(i2c_fd, MODULE_ADDRESSES)) {
        std::cerr << "Failed to initialize display modules." << std::endl;
        close(i2c_fd);
        return 1;
    }
    cv::VideoCapture cap(video_path, cv::CAP_FFMPEG);
    if (!cap.isOpened()) { std::cerr << "動画ファイルを開けません: " << video_path << std::endl; close(i2c_fd); return 1; }

    cv::Size virtual_display_size;
    float target_aspect;
    if (orientation == DisplayOrientation::LANDSCAPE) {
        virtual_display_size = cv::Size(DISPLAYS_ACROSS * CELL_WIDTH, DISPLAYS_DOWN * CELL_HEIGHT);
        target_aspect = (float)virtual_display_size.width / virtual_display_size.height;
    } else {
        virtual_display_size = cv::Size(DISPLAYS_DOWN * CELL_WIDTH, DISPLAYS_ACROSS * CELL_HEIGHT);
        target_aspect = (float)virtual_display_size.width / virtual_display_size.height;
    }
    
    std::string command = "ffplay -nodisp -autoexit \"" + video_path + "\" > /dev/null 2>&1 &";
    system(command.c_str());
    
    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;
    auto frame_duration = std::chrono::microseconds(static_cast<long long>(1000000.0 / fps));
    std::cout << "再生開始: " << video_path << std::endl;
    
    auto next_frame_time = std::chrono::steady_clock::now();
    cv::Mat frame;
    while (!g_should_exit && !g_stop_current_video && cap.read(frame)) {
        cv::Mat cropped_frame;
        float source_aspect = (float)frame.cols / frame.rows;
        if (source_aspect > target_aspect) {
            int new_width = static_cast<int>(frame.rows * target_aspect); int x = (frame.cols - new_width) / 2;
            cropped_frame = frame(cv::Rect(x, 0, new_width, frame.rows));
        } else {
            int new_height = static_cast<int>(frame.cols / target_aspect); int y = (frame.rows - new_height) / 2;
            cropped_frame = frame(cv::Rect(0, y, frame.cols, new_height));
        }
        
        cv::Mat resized_frame, gray_frame, bw_frame;
        cv::resize(cropped_frame, resized_frame, virtual_display_size);
        cv::cvtColor(resized_frame, gray_frame, cv::COLOR_BGR2GRAY);
        cv::threshold(gray_frame, bw_frame, 128, 255, cv::THRESH_BINARY);
        
        std::vector<uint8_t> grid = frame_to_grid_data(bw_frame, orientation);
        update_display(i2c_fd, grid, MODULE_ADDRESSES);
        
        next_frame_time += frame_duration;
        std::this_thread::sleep_until(next_frame_time);
    }
    
    system("killall ffplay > /dev/null 2>&1");
    cap.release();
    close(i2c_fd);
    if (g_stop_current_video) { std::cout << "再生中止: " << video_path << std::endl; }
    else { std::cout << "再生終了: " << video_path << std::endl; }
    return 0;
}

