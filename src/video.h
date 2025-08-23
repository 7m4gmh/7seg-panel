#pragma once
#include <opencv2/opencv.hpp>
void frame_to_grid(const cv::Mat& bw, std::vector<uint8_t>& grid);

void video_thread(int i2c_fd);

