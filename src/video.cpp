// src/video.cpp 
#include "common.h"
#include "led.h"
#include "video.h"
#include <thread>
#include <chrono>
#include <map>
#ifndef __APPLE__
#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp> // imdecode 用を明示
#endif
#include <fcntl.h>  // open() と O_RDWR のために必要
#include <unistd.h> // close(), usleep() のために必要

// 7セグ1文字内のピクセルサンプリング位置 (変更なし)
const std::map<int, std::pair<int, int>> SEG_MAP = {
    {0, {1, 0}}, {1, {3, 1}}, {2, {3, 3}}, {3, {1, 4}},
    {4, {0, 3}}, {5, {0, 1}}, {6, {1, 2}}, {7, {4, 4}}
};


#include <iostream> 


void frame_to_grid(const cv::Mat& bw, const DisplayConfig& config, std::vector<uint8_t>& grid) {
    // --- ステップ1: ディスプレイの物理アスペクト比を元に、高解像度フレームから切り取るべき領域(ROI)を計算 ---
    const double display_aspect_ratio = 
        (config.total_width * CHAR_WIDTH_MM) / (config.total_height * CHAR_HEIGHT_MM);
    const double frame_aspect_ratio = (double)bw.cols / (double)bw.rows;

    cv::Rect roi; // サンプリング領域 (x, y, width, height)
    if (frame_aspect_ratio > display_aspect_ratio) {
        // 映像がディスプレイより「横長」の場合 -> 左右をクロップ
        int new_width = std::min(bw.cols - 1, static_cast<int>(bw.rows * display_aspect_ratio));
        roi = cv::Rect((bw.cols - new_width) / 2, 0, new_width, bw.rows - 1);
    } else {
        // 映像がディスプレイより「縦長」の場合 -> 上下をクロップ
        int new_height = std::min(bw.rows - 1, static_cast<int>(bw.cols / display_aspect_ratio));
        roi = cv::Rect(0, (bw.rows - new_height) / 2, bw.cols - 1, new_height);
    }

    // --- ステップ2: 算出したROIを基準に、各文字・各セグメントのサンプリング座標を計算 ---
    const int TOTAL_WIDTH = config.total_width;
    const int TOTAL_HEIGHT = config.total_height;
    grid.assign(config.total_digits(), 0);

    // ROI（サンプリング領域）内での「1文字分」のセルの大きさをピクセル単位で計算
    const double cell_width_in_roi = (double)roi.width / TOTAL_WIDTH;
    const double cell_height_in_roi = (double)roi.height / TOTAL_HEIGHT;

    for (int char_r = 0; char_r < TOTAL_HEIGHT; ++char_r) {
        for (int char_c = 0; char_c < TOTAL_WIDTH; ++char_c) {
            
            // この文字の左上の基準座標を、元の高解像度フレーム上の絶対座標として計算
            const double base_x = roi.x + char_c * cell_width_in_roi;
            const double base_y = roi.y + char_r * cell_height_in_roi;

            uint8_t seg = 0;
            for (auto const& [bit, pos] : SEG_MAP) {
                auto [dx, dy] = pos; // SEG_MAP内の抽象的な座標(0-4)

                // 抽象的な5x5グリッド座標を、物理的な縦横比を持つcellの大きさに合わせてスケーリング
                int px_offset = static_cast<int>((dx / 4.0) * cell_width_in_roi);
                int py_offset = static_cast<int>((dy / 4.0) * cell_height_in_roi);
                
                int px = static_cast<int>(base_x) + px_offset;
                int py = static_cast<int>(base_y) + py_offset;
                
                if (px >= 0 && px < bw.cols && py >= 0 && py < bw.rows && bw.at<uint8_t>(py, px) > 128) {
                    seg |= (1 << bit);
                }
            }
            grid[char_r * TOTAL_WIDTH + char_c] = seg;
        }
    }
}



// ★修正1★ 引数を「値渡し」から「参照渡し」に変更 (int -> int&)
// これにより、関数内で i2c_fd を再オープンした結果が呼び出し元に反映される
void video_thread(int& i2c_fd, const DisplayConfig& config, std::atomic<bool>& stop_flag) {
#ifdef __APPLE__
    // MacではOpenCVがないので、スタブ
    (void)i2c_fd;
    (void)config;
    (void)stop_flag;
    std::cout << "Video thread disabled on macOS (no OpenCV)" << std::endl;
    while (!stop_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
#else
    std::vector<uint8_t> grid(config.total_digits(), 0);
    auto frame_duration = std::chrono::milliseconds(1000 / FPS);

    static int dbg_count = 0;

    while (!stop_flag) {
        auto start_time = std::chrono::steady_clock::now();
        
        std::vector<uint8_t> frame_data;

        {
            std::lock_guard<std::mutex> lock(frame_mtx);
            if (!latest_frame.empty()) {
                frame_data = latest_frame; // JPEG バイト列
            }
        }

        if (!frame_data.empty()) {
            // JPEG をグレースケールにデコード
            cv::Mat decoded = cv::imdecode(frame_data, cv::IMREAD_GRAYSCALE);
            if (decoded.empty()) {
                std::cerr << "[video] JPEG decode failed (size=" << frame_data.size() << " bytes)" << std::endl;
            } else {
                if (++dbg_count <= 3) {
                    std::cout << "[video] decoded frame: " << decoded.cols << "x" << decoded.rows << " ("
                              << frame_data.size() << " bytes)" << std::endl;
                }
                frame_to_grid(decoded, config, grid);
                I2CErrorInfo error_info; 

                if (!update_flexible_display(i2c_fd, config, grid, error_info)) {
                    if (!attempt_i2c_recovery(i2c_fd, config)) {
                        // 全ての復旧に失敗した場合、長めに待つ
                        std::cerr << "Recovery failed in video_thread. Pausing before next attempt..." << std::endl;
                        sleep(2);
                    }                                  
                }     
            }
        }

        // sleep_until と sleep_for の混在をやめ、シンプルな形でフレームレートを維持する
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        auto wait_time = frame_duration - elapsed_time;
        if (wait_time.count() > 0) {
            std::this_thread::sleep_for(wait_time);
        }
    }
#endif
}

/* 
#include <opencv2/imgproc.hpp> // cv::circle のために追加

void debug_draw_sampling_points(const DisplayConfig& config, cv::Mat& output_frame) {
    // この関数は frame_to_grid とほぼ同じロジックでサンプリング座標を計算し、
    // 実際にピクセルを読む代わりに、その位置に点を描画します。

    // --- 1. アスペクト比と仮想領域の計算 (frame_to_gridと全く同じ) ---
    const double display_aspect_ratio = (config.total_width * CHAR_WIDTH_MM) / (config.total_height * CHAR_HEIGHT_MM);
    const double frame_aspect_ratio = (double)output_frame.cols / (double)output_frame.rows;
    int roi_x, roi_y, roi_w, roi_h;
    if (frame_aspect_ratio > display_aspect_ratio) {
        roi_h = output_frame.rows;
        roi_w = static_cast<int>(output_frame.rows * display_aspect_ratio);
        roi_x = (output_frame.cols - roi_w) / 2;
        roi_y = 0;
    } else {
        roi_w = output_frame.cols;
        roi_h = static_cast<int>(output_frame.cols / display_aspect_ratio);
        roi_x = 0;
        roi_y = (output_frame.rows - roi_h) / 2;
    }

    // --- 2. 仮想領域の範囲を矩形で描画 ---
    cv::rectangle(output_frame, cv::Point(roi_x, roi_y), cv::Point(roi_x + roi_w, roi_y + roi_h), cv::Scalar(0, 255, 0), 1); // 緑色の矩形

    // --- 3. 各サンプリングポイントを点で描画 ---
    const int TOTAL_WIDTH = config.total_width;
    const int TOTAL_HEIGHT = config.total_height;

    for (int i = 0; i < config.total_digits(); i++) {
        int r = i / TOTAL_WIDTH;
        int c = i % TOTAL_WIDTH;
        
        int xs = roi_x + static_cast<int>(c * (roi_w / (double)TOTAL_WIDTH));
        int ys = roi_y + static_cast<int>(r * (roi_h / (double)TOTAL_HEIGHT));
        
        for (auto const& [bit, pos] : SEG_MAP) {
            auto [dx, dy] = pos;
            int px = xs + dx;
            int py = ys + dy;
            
            // サンプリング座標に赤い点を描画
            if (px >= 0 && px < output_frame.cols && py >= 0 && py < output_frame.rows) {
                // 小さな円を描画
                cv::circle(output_frame, cv::Point(px, py), 1, cv::Scalar(0, 0, 255), -1); // 赤い点
            }
        }
    }
}
*/