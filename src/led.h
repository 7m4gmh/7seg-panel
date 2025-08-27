// src/led.h
#pragma once

#include "config.h" // 新しくインクルード
#include <vector>
#include <cstdint> 

// 1つのモジュールにデータを書き込む内部関数 (変更なし)
void update_module_from_grid(int i2c_bus_fd, int addr, const std::vector<uint8_t>& grid16);

// --- 変更 ---
// パネル全体を描画する関数
// 古い関数: void update_display(int i2c_fd, const std::vector<uint8_t>& grid, const std::vector<int>& module_addrs);
// 新しい関数:
void update_flexible_display(int i2c_fd, const DisplayConfig& config, const std::vector<uint8_t>& grid);

// initialize_displaysの宣言は common.h にあるため、ここでは不要

