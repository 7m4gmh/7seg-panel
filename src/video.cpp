// src/video.cpp 
#include "common.h"
#include "led.h"
#include "video.h"
#include <thread>
#include <chrono>
#include <map>
#include <opencv2/opencv.hpp>

// 7セグ1文字内のピクセルサンプリング位置 (変更なし)
const std::map<int, std::pair<int, int>> SEG_MAP = {
    {0, {1, 0}}, {1, {3, 1}}, {2, {3, 3}}, {3, {1, 4}},
    {4, {0, 3}}, {5, {0, 1}}, {6, {1, 2}}, {7, {4, 4}}
};


void frame_to_grid(const cv::Mat& bw, const DisplayConfig& config, std::vector<uint8_t>& grid) {
    const int TOTAL_DIGITS = config.total_digits();
    const int TOTAL_WIDTH = config.total_width;
    const int TOTAL_HEIGHT = config.total_height;
    
    const int FRAME_W = W; 
    const int FRAME_H = H;

    grid.assign(TOTAL_DIGITS, 0);

    for (int i = 0; i < TOTAL_DIGITS; i++) {
        int r = i / TOTAL_WIDTH;
        // ★★★ バグ修正: ここは TOTAL_WIDTH で割るのが正しいです ★★★
        int c = i % TOTAL_WIDTH;
        
        int xs = c * (FRAME_W / TOTAL_WIDTH);
        int ys = r * (FRAME_H / TOTAL_HEIGHT);
        
        uint8_t seg = 0;
        for (auto const& [bit, pos] : SEG_MAP) {
            auto [dx, dy] = pos;
            int px = xs + dx;
            int py = ys + dy;
            if (px >= 0 && px < FRAME_W && py >= 0 && py < FRAME_H && bw.at<uint8_t>(py, px) > 128) {
                seg |= (1 << bit);
            }
        }
        grid[i] = seg;
    }
}


void video_thread(int i2c_fd, const DisplayConfig& config) {
    // ★★★ 修正点: current_frame はループ内で生成するため、ここでは不要 ★★★
    std::vector<uint8_t> grid(config.total_digits(), 0);
    auto frame_duration = std::chrono::milliseconds(1000 / FPS);

    while (!finished) {
        auto start_time = std::chrono::steady_clock::now();
        
        // ★★★ 修正点: vectorとしてデータを受け取り、cv::Matに変換する ★★★
        std::vector<uint8_t> frame_data;

        // 最新のフレームをロックしてローカルベクターにコピー
        {
            std::lock_guard<std::mutex> lock(frame_mtx);
            if (!latest_frame.empty()) {
                frame_data = latest_frame;
            }
        }

        // コピーしたフレームデータが有効な場合のみ処理を実行
        if (!frame_data.empty()) {
            // vectorのデータを指すcv::Matオブジェクトを作成 (データコピーは発生しない)
            cv::Mat current_frame(H, W, CV_8UC1, frame_data.data());

            // フレームを7セグのグリッドデータに変換
            frame_to_grid(current_frame, config, grid);

            // 新しい関数を使ってディスプレイを更新
            update_flexible_display(i2c_fd, config, grid);
        }

        auto end_time = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        auto wait_time = frame_duration - elapsed_time;
        if (wait_time.count() > 0) {
            std::this_thread::sleep_for(wait_time);
        }
    }
}


