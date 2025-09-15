// src/video.h
#pragma once
#ifndef VIDEO_H
#define VIDEO_H
#include "config.h" // 新しくインクルード
#include <opencv2/opencv.hpp>
#include <vector>   // std::vector のためにインクルード

void frame_to_grid(const cv::Mat& bw, const DisplayConfig& config, std::vector<uint8_t>& grid);
void video_thread(int& i2c_fd, const DisplayConfig& config, std::atomic<bool>& stop_flag);

#endif // VIDEO_H