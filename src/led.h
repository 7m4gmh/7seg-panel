// src/led.h
#pragma once

#include "config.h" 
#include <vector>
#include <cstdint> 
#include "common.h"

// 1つのモジュールにデータを書き込む内部関数 
bool update_module_from_grid(int i2c_bus_fd, int addr, const std::vector<uint8_t>& grid16, I2CErrorInfo& error_info_out);

// パネル全体を描画する関数
bool update_flexible_display(int i2c_fd, const DisplayConfig& config, const std::vector<uint8_t>& grid, I2CErrorInfo& error_info_out);

// I2C通信状態のキャッシュをリセットするための関数
void reset_i2c_channel_cache();

bool select_i2c_channel(int i2c_fd, int expander_addr, int channel, I2CErrorInfo& error_info_out);
