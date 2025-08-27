// src/common.h
#pragma once

// --- 変更 ---
// 新しい設定ファイルをインクルード
#include "config.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

#ifndef COMMON_H
#define COMMON_H

#include <cstdint>
#include <csignal>

// --- 削除 ---
// extern std::vector<int>  module_addrs; // DisplayConfigに統合
// extern const std::vector<int> MODULE_ADDRESSES; // DisplayConfigに統合

// Ctrl+C / SIGTERM で立つ終了フラグ
extern volatile sig_atomic_t g_should_exit;

// シグナルハンドラ設定用
void setup_signal_handlers();

// --- 変更 ---
// 関数のシグネチャをDisplayConfigを受け取るように変更
bool initialize_displays(int i2c_fd, const DisplayConfig& config);

#endif

// --- アプリケーション設定値 (ディスプレイ構成とは直接関係ないため維持) ---
constexpr int PORT = 9999;
constexpr int W = 96;
constexpr int H = 20;
constexpr int FPS = 15;
constexpr int AUDIO_CHUNK_SIZE = 2048;
constexpr int FRAMES_PER_BUFFER = 1024;
constexpr int SAMPLE_RATE = 44100;
constexpr int CHANNELS = 2; // 1 for monoral, 2 for stereo

// --- 削除 ---
// 以下の定数はDisplayConfigから取得するため不要
// constexpr int ACROSS = 24;
// constexpr int DOWN = 4;
// constexpr int TOTAL = ACROSS * DOWN;

// --- グローバル変数 (変更なし) ---
extern std::atomic<bool> finished;
extern std::atomic<bool> audio_waiting;
extern std::mutex audio_mtx;
extern std::deque<std::vector<char>> audio_buf;

extern std::mutex frame_mtx;
extern std::vector<uint8_t> latest_frame;
extern std::atomic<int> last_pts_ms;
extern double start_time;
extern int audio_bytes_received;



