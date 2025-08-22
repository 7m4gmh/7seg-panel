#pragma once
#include <atomic>
#include <deque>
#include <mutex>
#include <vector>

// --- 設定値 ---
constexpr int PORT = 9999;
constexpr int W = 96;
constexpr int H = 20;
constexpr int FPS = 15;
constexpr int AUDIO_CHUNK_SIZE = 2048;
constexpr int SAMPLE_RATE = 44100;
constexpr int CHANNELS = 2;
constexpr int ACROSS = 24;
constexpr int DOWN = 4;
constexpr int TOTAL = ACROSS * DOWN;

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
