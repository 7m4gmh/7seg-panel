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
