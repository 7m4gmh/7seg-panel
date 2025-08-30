// src/led.cpp
#include "led.h"
#include "common.h"
#include <vector>
#include <map>
#include <cstring>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <cstdio>
#include <cstdint>
#include <iostream>

// 現在選択されているチャンネルを記憶する (-1は未選択/直接接続)
static int g_current_channel = -2; // 初期値を-2にして初回は必ず設定されるようにする

void select_i2c_channel(int i2c_fd, int expander_addr, int channel) {
    if (expander_addr < 0) {
        g_current_channel = -1;
        return;
    }
    if (channel == g_current_channel) {
        return;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, expander_addr) < 0) {
        fprintf(stderr, "ERROR: ioctl I2C_SLAVE for TCA9548A (0x%02X) failed\n", expander_addr);
        perror("ioctl");
        return;
    }

    uint8_t cmd = (channel < 0) ? 0x00 : (1 << channel);
    if (write(i2c_fd, &cmd, 1) != 1) {
        fprintf(stderr, "ERROR: Failed to write to TCA9548A (0x%02X) to select channel %d\n", expander_addr, channel);
        perror("write");
    } else {
        // ★★★ デバッグログ ★★★
       // std::cout << "[DEBUG] Switched I2C expander to Channel " << channel << std::endl;
    }
    
    g_current_channel = channel;
    usleep(1000); 
}

bool initialize_displays(int i2c_fd, const DisplayConfig& config) {
    std::cout << "Initializing modules..." << std::endl;
    
    // ★★★ TCAなしの場合は、古いロジックに戻す ★★★
    if (config.tca9548a_address < 0) {
        std::vector<int> module_addrs = config.all_addresses();
        for (int addr : module_addrs) {
            if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0) {
                perror("ioctl I2C_SLAVE failed during initialization");
                return false;
            }
            uint8_t commands[] = { 0x21, 0x81, 0xEF };
            for (uint8_t cmd : commands) {
                if (write(i2c_fd, &cmd, 1) != 1) {
                    fprintf(stderr, "ERROR [Init]: Failed to write command 0x%02X to address 0x%02X\n", cmd, addr);
                    perror(" -> i2c write command");
                    // return false; // エラーなら初期化を中断した方が良い
              }
                usleep(1000);
            }
        }
    } else {
        // ★★★ TCAありの場合は、新しいロジックを使う ★★★
        for (auto const& [channel, grid] : config.channel_grids) {
            select_i2c_channel(i2c_fd, config.tca9548a_address, channel);
            usleep(5000); // ★★★ チャンネル切り替え後、少し長めに待機してみる (5ms) ★★★
            for (const auto& row : grid) {
                for (int addr : row) {
                    if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0) {
                        perror("ioctl I2C_SLAVE failed during initialization");
                        return false;
                    }
                    uint8_t commands[] = { 0x21, 0x81, 0xEF };
                    for (uint8_t cmd : commands) {
    if (write(i2c_fd, &cmd, 1) != 1) {
        // どのチャンネルのどのアドレスで失敗したかを出力すると、より分かりやすい
        fprintf(stderr, "ERROR [Init]: Failed to write command 0x%02X to CH%d addr 0x%02X\n", cmd, channel, addr);
        perror(" -> i2c write command");
    }
    usleep(1000);
}
                    for (uint8_t cmd : commands) {
                        if (write(i2c_fd, &cmd, 1) != 1) {
                            fprintf(stderr, "Failed to write command 0x%02X to address 0x%02X\n", cmd, addr);
                            perror("i2c write command");
                        }
                        usleep(1000);
                    }
                }
            }
        }
        select_i2c_channel(i2c_fd, config.tca9548a_address, -1);
    }
    
    std::cout << "Initialization complete." << std::endl;
    return true;
}

void update_module_from_grid(int i2c_bus_fd, int addr, const std::vector<uint8_t>& grid16) {
    uint8_t display_buffer[16] = {0};
    const int DIGITS_PER_MODULE = 16; 
    for (int digit_index = 0; digit_index < DIGITS_PER_MODULE && static_cast<size_t>(digit_index) < grid16.size(); ++digit_index) {
        uint8_t bitmask = grid16[digit_index];
        for (int seg = 0; seg < 8; ++seg) {
            if ((bitmask >> seg) & 1) {
                int base = seg * 2;
                int addr_to_write, bit_pos;
                if (digit_index < 8) {
                    addr_to_write = base; bit_pos = digit_index;
                } else {
                    addr_to_write = base + 1; bit_pos = digit_index - 8;
                }
                if (addr_to_write < 16) {
                    display_buffer[addr_to_write] |= (1 << bit_pos);
                }
            }
        }
    }
    if (ioctl(i2c_bus_fd, I2C_SLAVE, addr) < 0) {
        perror("ioctl I2C_SLAVE"); return;
    }
    uint8_t buf[17];
    buf[0] = 0x00;
    memcpy(buf + 1, display_buffer, 16);
    if (write(i2c_bus_fd, buf, 17) != 17) {
        fprintf(stderr, "ERROR: Failed to write display data to module at address 0x%02X\n", addr);
        perror(" -> i2c write"); // perrorの前にどの処理かを示すと更に分かりやすい
    }
}

/**
 * @brief 物理レイアウトを元にディスプレイ全体を更新する (修正版)
 * 
 * @param i2c_fd I2Cファイルディスクリプタ
 * @param config ディスプレイの物理構成
 * @param grid 表示データ（左上から右下への一次元配列）
 */
// src/led.cpp

void update_flexible_display(int i2c_fd, const DisplayConfig& config, const std::vector<uint8_t>& grid) {
    bool use_tca = (config.tca9548a_address != -1);
    const int digits_per_module = config.module_digits_width * config.module_digits_height;
    std::vector<uint8_t> module_data_buffer(digits_per_module);
    int global_row_offset = 0;
    int global_col_offset = 0;

    for (const auto& [channel, address_grid] : config.channel_grids) {
        if (use_tca) {
            select_i2c_channel(i2c_fd, config.tca9548a_address, channel);
        }
        if (address_grid.empty()) continue;
        const int channel_grid_height = address_grid.size();
        const int channel_grid_width = address_grid[0].size();

        for (int grid_r = 0; grid_r < channel_grid_height; ++grid_r) {
            for (int grid_c = 0; grid_c < channel_grid_width; ++grid_c) {
                int module_addr = address_grid[grid_r][grid_c];
                int module_start_col = global_col_offset + (grid_c * config.module_digits_width);
                int module_start_row = global_row_offset + (grid_r * config.module_digits_height);

                for (int r_in_mod = 0; r_in_mod < config.module_digits_height; ++r_in_mod) {
                    for (int c_in_mod = 0; c_in_mod < config.module_digits_width; ++c_in_mod) {
                        int total_grid_col = module_start_col + c_in_mod;
                        int total_grid_row = module_start_row + r_in_mod;
                        int grid_index = total_grid_row * config.total_width + total_grid_col;
                        int module_buffer_index = r_in_mod * config.module_digits_width + c_in_mod;
                        if (static_cast<size_t>(grid_index) < grid.size() && static_cast<size_t>(module_buffer_index) < module_data_buffer.size()) {
                            module_data_buffer[module_buffer_index] = grid[grid_index];
                        }
                    }
                }
                update_module_from_grid(i2c_fd, module_addr, module_data_buffer);
            }
        }
        
        int channel_width_in_digits = channel_grid_width * config.module_digits_width;
        int channel_height_in_digits = channel_grid_height * config.module_digits_height;
        if ((global_col_offset + channel_width_in_digits) < config.total_width) {
            global_col_offset += channel_width_in_digits;
        } else {
            global_col_offset = 0;
            global_row_offset += channel_height_in_digits;
        }
    }
}