// src/video.h
#pragma once

#include "config.h" // 新しくインクルード
#include <opencv2/opencv.hpp>
#include <vector>   // std::vector のためにインクルード

// --- 変更 ---
// 関数のシグネチャをDisplayConfigを受け取るように変更
void frame_to_grid(const cv::Mat& bw, const DisplayConfig& config, std::vector<uint8_t>& grid);

void video_thread(int i2c_fd, const DisplayConfig& config);

