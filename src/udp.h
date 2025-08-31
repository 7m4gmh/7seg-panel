// src/udp.h
#ifndef UDP_H
#define UDP_H
#include "common.h"
#include <atomic>
// エンジンから呼び出される関数の宣言
void start_udp_server(int i2c_fd, int port, const DisplayConfig& config, std::atomic<bool>& stop_flag);
#endif // UDP_H