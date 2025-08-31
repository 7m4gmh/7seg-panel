// src/common.h
#pragma once

#include "config.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <csignal>

// Ctrl+C / SIGTERM で立つ終了フラグ
//extern volatile sig_atomic_t g_should_exit;
extern volatile std::sig_atomic_t g_should_exit;

// シグナルハンドラ設定用
void setup_signal_handlers();

// 関数のシグネチャをDisplayConfigを受け取るように変更
bool initialize_displays(int i2c_fd, const DisplayConfig& config);

#endif

// --- アプリケーション設定値  ---
constexpr int PORT = 9999;
const int W = 640;
const int H = 480;
constexpr int FPS = 15;
constexpr int AUDIO_CHUNK_SIZE = 2048;
constexpr int FRAMES_PER_BUFFER = 1024;
constexpr int SAMPLE_RATE = 44100;
constexpr int CHANNELS = 2; // 1 for monoral, 2 for stereo

// --- グローバル変数 ---
extern std::atomic<bool> finished;
extern std::atomic<bool> audio_waiting;
extern std::mutex audio_mtx;
extern std::deque<std::vector<char>> audio_buf;

extern std::mutex frame_mtx;
extern std::vector<uint8_t> latest_frame;
extern std::atomic<int> last_pts_ms;
extern double start_time;
extern int audio_bytes_received;



