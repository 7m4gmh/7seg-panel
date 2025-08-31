#include "common.h"

std::atomic<bool> finished(false);
std::atomic<bool> audio_waiting(false);
std::mutex audio_mtx;
std::deque<std::vector<char>> audio_buf;

std::mutex frame_mtx;
std::vector<uint8_t> latest_frame;
std::atomic<int> last_pts_ms(0);
double start_time = 0.0;
int audio_bytes_received = 0;

// 共通モジュール I²C アドレス
//const std::vector<int> MODULE_ADDRESSES = {0x70, 0x71, 0x72, 0x73, 0x74, 0x75};

volatile std::sig_atomic_t g_should_exit = 0;
//volatile sig_atomic_t g_should_exit = 0;

double CHAR_WIDTH_MM = 12.7;
double CHAR_HEIGHT_MM = 19.2;

// シグナルハンドラ関数
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_should_exit = 1;
    }
}

// シグナルハンドラを設定する関数
void setup_signal_handlers() {
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}
