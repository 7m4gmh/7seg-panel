// src/led.cpp
#include "led.h"
#include "common.h"
#include <vector>
#include <map>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#ifndef __APPLE__
#include <linux/i2c-dev.h>
#endif
#include <cstdio>
#include <cstdint>
#include <iostream>

// 現在選択されているチャンネルを記憶する (-1は未選択/直接接続)
static int g_current_channel = -2; // 初期値を-2にして初回は必ず設定されるようにする

void reset_i2c_channel_cache() {
    g_current_channel = -2; // 初期値に戻す
}



// ★変更★ 戻り値の型を void から bool に変更
bool select_i2c_channel(int i2c_fd, int expander_addr, int channel, I2CErrorInfo& error_info_out) {
#ifdef __APPLE__
    // MacではI2Cがないので、スタブ
    (void)i2c_fd;
    (void)expander_addr;
    (void)channel;
    (void)error_info_out;
    return true;
#else
    if (expander_addr < 0) {
        g_current_channel = -1;
        return true; // TCAがない場合は常に成功
    }
    if (channel == g_current_channel) {
        return true; // チャンネル変更が不要な場合は成功
    }

    if (ioctl(i2c_fd, I2C_SLAVE, expander_addr) < 0) {
        fprintf(stderr, "ERROR: ioctl I2C_SLAVE for TCA9548A (0x%02X) failed\n", expander_addr);
        perror("ioctl");
        error_info_out.error_occurred = true;
        error_info_out.channel = channel;
        error_info_out.address = expander_addr; // エラーはエキスパンダ自体で発生
        return false; // ★変更★ エラー時に false を返す
    }
    uint8_t cmd = (channel < 0) ? 0x00 : (1 << channel);
    if (write(i2c_fd, &cmd, 1) != 1) {
        fprintf(stderr, "ERROR: Failed to write to TCA9548A (0x%02X) to select channel %d\n", expander_addr, channel);
        perror("write");
        error_info_out.error_occurred = true;
        error_info_out.channel = channel;
        error_info_out.address = expander_addr;
        return false; // ★変更★ エラー時に false を返す
    }
    
    g_current_channel = channel;
    usleep(1000); 
    return true; // ★変更★ 成功時に true を返す
#endif
}


bool initialize_displays(int i2c_fd, const DisplayConfig& config) {
#ifdef __APPLE__
    // MacではI2Cがないので、スタブ
    (void)i2c_fd;
    (void)config;
    std::cout << "Initialization skipped on macOS (no I2C)" << std::endl;
    return true;
#else
    std::cout << "Initializing modules..." << std::endl;
    I2CErrorInfo dummy_error_info; // ★★★ ダミーの変数を定義 ★★★

    for (const auto& [bus_id, bus_config] : config.buses) {
        for (const auto& tca : bus_config.tca9548as) {
            bool use_tca = (tca.address != -1);
            for (const auto& [channel, grid] : tca.channels) {
                if (use_tca) {
                    // チャンネル切り替えに失敗したら、即座に中断
                    if (!select_i2c_channel(i2c_fd, tca.address, channel, dummy_error_info)) {
                        fprintf(stderr, "ERROR [Init]: Failed to select channel %d on TCA 0x%02X\n", channel, tca.address);
                        return false;
                    }
                    usleep(5000);
                }

                for (const auto& row : grid) {
                    for (int addr : row) {
                        if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0) {
                            perror("ioctl I2C_SLAVE failed during initialization");
                            return false;
                        }
                        uint8_t commands[] = { 0x21, 0x81, 0xEF };
                        for (uint8_t cmd : commands) {
                            if (write(i2c_fd, &cmd, 1) != 1) {
                                fprintf(stderr, "ERROR [Init]: Failed to write command 0x%02X to CH%d addr 0x%02X\n", cmd, channel, addr);
                                perror(" -> i2c write command");
                                // エラー発生時に即座に false を返す
                                return false; 
                            }
                            usleep(1000);
                        }
                    }
                }
            }
            if (use_tca) {
                // 全チャンネルを無効化
                if (!select_i2c_channel(i2c_fd, tca.address, -1, dummy_error_info)) {
                    fprintf(stderr, "ERROR [Init]: Failed to disable all channels on TCA9548A 0x%02X\n", tca.address);
                    return false;
                }
            }
        }
    }
    
    std::cout << "Initialization complete." << std::endl;
    return true; 
#endif
}


bool update_module_from_grid(int i2c_bus_fd, int addr, const std::vector<uint8_t>& grid16, I2CErrorInfo& error_info_out) {
#ifdef __APPLE__
    // MacではI2Cがないので、スタブ
    (void)i2c_bus_fd;
    (void)addr;
    (void)grid16;
    (void)error_info_out;
    return true;
#else
    // ... バッファ作成のロジック...
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
        perror("ioctl I2C_SLAVE");
        error_info_out.error_occurred = true;
        error_info_out.address = addr;
        return false; // ★変更★ エラー時に false を返す
    }

    uint8_t buf[17];
    buf[0] = 0x00;
    memcpy(buf + 1, display_buffer, 16);
    if (write(i2c_bus_fd, buf, 17) != 17) {
        fprintf(stderr, "ERROR: Failed to write display data to module at address 0x%02X\n", addr);
        perror(" -> i2c write");
        error_info_out.error_occurred = true;
        error_info_out.address = addr;
        return false; // ★変更★ エラー時に false を返す
    }
    return true; // ★変更★ 成功時に true を返す
#endif
}


/**
 * @brief 物理レイアウトを元にディスプレイ全体を更新する (修正版)
 * 
 * @param i2c_fd I2Cファイルディスクリプタ
 * @param config ディスプレイの物理構成
 * @param grid 表示データ（左上から右下への一次元配列）
 */


bool update_flexible_display(int i2c_fd, const DisplayConfig& config, const std::vector<uint8_t>& grid, I2CErrorInfo& error_info_out) {
#ifdef __APPLE__
    // MacではI2Cがないので、スタブ
    (void)i2c_fd;
    (void)config;
    (void)grid;
    (void)error_info_out;
    return true;
#else
    (void)0; // placeholder to avoid unused warnings
    int global_row_offset = 0;
    int global_col_offset = 0;

    for (const auto& [bus_id, bus_config] : config.buses) {
        // Bus ごとにオフセットをリセット
        int bus_row_offset = 0;
        int bus_col_offset = 0;
        for (const auto& tca : bus_config.tca9548as) {
            bool use_tca = (tca.address != -1);
            // 48x8の場合：rowsベースの処理
            if (config.total_width == 48 && config.total_height == 8 && !tca.rows.empty()) {
                if (getenv("DEBUG_LED")) {
                    printf("DEBUG: Using rows-based processing, rows.size()=%zu\n", tca.rows.size());
                    printf("DEBUG: total_width=%d, total_height=%d\n", config.total_width, config.total_height);
                }
                for (const auto& [row_id, row_config] : tca.rows) {
                    auto [channel, row_offset, col_offset] = row_config;
                    if (getenv("DEBUG_LED")) printf("DEBUG: Processing row_id=%d, channel=%d, row_offset=%d, col_offset=%d\n", row_id, channel, row_offset, col_offset);
                    if (tca.channels.find(channel) == tca.channels.end()) continue;
                    const auto& address_grid = tca.channels.at(channel);
                    
                    if (use_tca) {
                        if (!select_i2c_channel(i2c_fd, tca.address, channel, error_info_out)) {
                            return false;
                        }
                    }
                    
                    if (address_grid.empty()) continue;
                    const int channel_grid_height = address_grid.size();
                    const int channel_grid_width = address_grid[0].size();

                    // Precompute per-module widths/heights for this address grid
                    std::vector<std::vector<int>> mod_widths(channel_grid_height, std::vector<int>(channel_grid_width));
                    std::vector<std::vector<int>> mod_heights(channel_grid_height, std::vector<int>(channel_grid_width));
                    for (int rr = 0; rr < channel_grid_height; ++rr) {
                        for (int cc = 0; cc < channel_grid_width; ++cc) {
                            int maddr = address_grid[rr][cc];
                            auto [mw, mh] = config.module_size_for_address(maddr);
                            mod_widths[rr][cc] = mw;
                            mod_heights[rr][cc] = mh;
                        }
                    }

                    for (int grid_r = 0; grid_r < channel_grid_height; ++grid_r) {
                        for (int grid_c = 0; grid_c < channel_grid_width; ++grid_c) {
                            int module_addr = address_grid[grid_r][grid_c];
                            // compute start col by summing widths of previous modules in the same row
                            int col_sum = 0;
                            for (int pc = 0; pc < grid_c; ++pc) col_sum += mod_widths[grid_r][pc];
                            int module_start_col = col_offset + bus_col_offset + col_sum;
                            // compute start row by summing heights of previous rows at this column
                            int row_sum = 0;
                            for (int pr = 0; pr < grid_r; ++pr) row_sum += mod_heights[pr][grid_c];
                            int module_start_row = row_offset + bus_row_offset + row_sum;

                            int mod_w = mod_widths[grid_r][grid_c];
                            int mod_h = mod_heights[grid_r][grid_c];
                            std::vector<uint8_t> local_module_buffer(mod_w * mod_h);

                            for (int r_in_mod = 0; r_in_mod < mod_h; ++r_in_mod) {
                                for (int c_in_mod = 0; c_in_mod < mod_w; ++c_in_mod) {
                                    int total_grid_col = global_col_offset + module_start_col + c_in_mod;
                                    int total_grid_row = global_row_offset + module_start_row + r_in_mod;
                                    int grid_index = total_grid_row * config.total_width + total_grid_col;
                                    int use_c = c_in_mod;
                                    bool reverse_cols = config.module_columns_reversed(module_addr);
                                    if (reverse_cols) use_c = (mod_w - 1 - c_in_mod);
                                    int module_buffer_index = r_in_mod * mod_w + use_c;
                                    if (static_cast<size_t>(grid_index) < grid.size() && static_cast<size_t>(module_buffer_index) < local_module_buffer.size()) {
                                        local_module_buffer[module_buffer_index] = grid[grid_index];
                                    }
                                }
                            }

                            if (!update_module_from_grid(i2c_fd, module_addr, local_module_buffer, error_info_out)) {
                                fprintf(stderr, "Failed to update module at CH%d addr 0x%02X. Aborting update.\n", channel, module_addr);
                                error_info_out.channel = channel; 
                                return false;
                            }
                        }
                    }
                }
            } else {
                // 従来のチャンネルベースの処理
                for (const auto& [channel, address_grid] : tca.channels) {
                    if (use_tca) {
                        // ★変更★ select_i2c_channel の戻り値を確認
                        if (!select_i2c_channel(i2c_fd, tca.address, channel, error_info_out)) {
                            return false;
                        }
                    }
                    if (address_grid.empty()) continue;
                    const int channel_grid_height = address_grid.size();
                    const int channel_grid_width = address_grid[0].size();

                    // 48x8の場合の特殊処理：チャンネル番号に基づいてオフセットを決定
                    int channel_row_offset = 0;
                    int channel_col_offset = 0;
                    if (config.total_width == 48 && config.total_height == 8) {
                        // チャンネル0,2：上半分（行0-3）、チャンネル1,3：下半分（行4-7）
                        channel_row_offset = (channel % 2) * 4;
                        // チャンネル0,1：左半分（列0-23）、チャンネル2,3：右半分（列24-47）
                        channel_col_offset = (channel / 2) * 24;
                        fprintf(stderr, "DEBUG: Channel %d -> row_offset=%d, col_offset=%d\n", channel, channel_row_offset, channel_col_offset);
                    }

                    // Precompute per-module widths/heights for this address grid
                    std::vector<std::vector<int>> mod_widths(channel_grid_height, std::vector<int>(channel_grid_width));
                    std::vector<std::vector<int>> mod_heights(channel_grid_height, std::vector<int>(channel_grid_width));
                    for (int rr = 0; rr < channel_grid_height; ++rr) {
                        for (int cc = 0; cc < channel_grid_width; ++cc) {
                            int maddr = address_grid[rr][cc];
                            auto [mw, mh] = config.module_size_for_address(maddr);
                            mod_widths[rr][cc] = mw;
                            mod_heights[rr][cc] = mh;
                        }
                    }

                    for (int grid_r = 0; grid_r < channel_grid_height; ++grid_r) {
                        for (int grid_c = 0; grid_c < channel_grid_width; ++grid_c) {
                            int module_addr = address_grid[grid_r][grid_c];
                            // compute start col by summing widths of previous modules in the same row
                            int col_sum = 0;
                            for (int pc = 0; pc < grid_c; ++pc) col_sum += mod_widths[grid_r][pc];
                            int module_start_col = channel_col_offset + bus_col_offset + col_sum;
                            // compute start row by summing heights of previous rows at this column
                            int row_sum = 0;
                            for (int pr = 0; pr < grid_r; ++pr) row_sum += mod_heights[pr][grid_c];
                            int module_start_row = channel_row_offset + bus_row_offset + row_sum;

                            int mod_w = mod_widths[grid_r][grid_c];
                            int mod_h = mod_heights[grid_r][grid_c];
                            std::vector<uint8_t> local_module_buffer(mod_w * mod_h);

                            for (int r_in_mod = 0; r_in_mod < mod_h; ++r_in_mod) {
                                for (int c_in_mod = 0; c_in_mod < mod_w; ++c_in_mod) {
                                    int total_grid_col = global_col_offset + module_start_col + c_in_mod;
                                    int total_grid_row = global_row_offset + module_start_row + r_in_mod;
                                    int grid_index = total_grid_row * config.total_width + total_grid_col;
                                    int use_c = c_in_mod;
                                    bool reverse_cols = config.module_columns_reversed(module_addr);
                                    if (reverse_cols) use_c = (mod_w - 1 - c_in_mod);
                                    int module_buffer_index = r_in_mod * mod_w + use_c;
                                    if (static_cast<size_t>(grid_index) < grid.size() && static_cast<size_t>(module_buffer_index) < local_module_buffer.size()) {
                                        local_module_buffer[module_buffer_index] = grid[grid_index];
                                    }
                                }
                            }

                            if (!update_module_from_grid(i2c_fd, module_addr, local_module_buffer, error_info_out)) {
                                fprintf(stderr, "Failed to update module at CH%d addr 0x%02X. Aborting update.\n", channel, module_addr);
                                error_info_out.channel = channel; 
                                return false; // エラーを伝播
                            }
                        }
                    }

                    // compute channel width/height in digits by summing per-module sizes
                    int channel_width_in_digits = 0;
                    for (int cc = 0; cc < channel_grid_width; ++cc) {
                        // assume top row's widths represent column widths
                        channel_width_in_digits += mod_widths[0][cc];
                    }
                    int channel_height_in_digits = 0;
                    for (int rr = 0; rr < channel_grid_height; ++rr) {
                        // assume left column's heights represent row heights
                        channel_height_in_digits += mod_heights[rr][0];
                    }
                    if ((bus_col_offset + channel_width_in_digits) < config.total_width) {
                        bus_col_offset += channel_width_in_digits;
                    } else {
                        bus_col_offset = 0;
                        bus_row_offset += channel_height_in_digits;
                    }
                }
            }
        }
        // Bus のオフセットをグローバルに反映
        if ((global_col_offset + bus_col_offset) < config.total_width) {
            global_col_offset += bus_col_offset;
        } else {
            global_col_offset = 0;
            global_row_offset += bus_row_offset;
        }
    }
    return true; // ★変更★ すべて成功したら true を返す
#endif
}


// 全モジュールを消灯する実装
bool clear_all_displays(const DisplayConfig& config) {
#ifdef __APPLE__
    (void)config;
    return true;
#else
    int fd = -1;
    // config からバスを選択して/dev/i2c-N を開く（common.cpp の open_i2c_auto と同様）
    if (!config.buses.empty()) {
        int bus_id = config.buses.begin()->first;
        std::string dev = "/dev/i2c-" + std::to_string(bus_id);
        fd = open(dev.c_str(), O_RDWR);
        if (fd >= 0) {
            std::cerr << "[I2C] using device: " << dev << std::endl;
        } else {
            std::cerr << "[I2C] Failed to open configured bus " << bus_id << ": " << dev << std::endl;
        }
    }
    if (fd < 0) {
        const char* cands[] = {"/dev/i2c-0", "/dev/i2c-1"};
        for (auto dev : cands) {
            fd = open(dev, O_RDWR);
            if (fd >= 0) {
                std::cerr << "[I2C] using device: " << dev << " (fallback)" << std::endl;
                break;
            }
        }
    }
    if (fd < 0) {
        perror("[I2C] clear_all_displays: open failed");
        return false;
    }

    I2CErrorInfo dummy;
    // 全モジュールに対して0表示を書き込む
    for (const auto& [bus_id, bus_config] : config.buses) {
        for (const auto& tca : bus_config.tca9548as) {
            bool use_tca = (tca.address != -1);
            for (const auto& [channel, address_grid] : tca.channels) {
                if (use_tca) {
                    if (!select_i2c_channel(fd, tca.address, channel, dummy)) {
                        std::cerr << "ERROR: Failed to select channel " << channel << " on TCA 0x" << std::hex << tca.address << std::dec << std::endl;
                        // 続行は試みるがエラーを記録
                    }
                }

                for (const auto& row : address_grid) {
                    for (int addr : row) {
                        if (ioctl(fd, I2C_SLAVE, addr) < 0) {
                            perror("ioctl I2C_SLAVE failed during clear_all_displays");
                            continue;
                        }
                        uint8_t buf[17];
                        buf[0] = 0x00; // register 0x00
                        memset(buf + 1, 0x00, 16);
                        if (write(fd, buf, 17) != 17) {
                            fprintf(stderr, "ERROR: Failed to write clear data to module at address 0x%02X\n", addr);
                            perror(" -> i2c write");
                        }
                        usleep(1000);
                    }
                }
            }
            if (use_tca) {
                // 全チャンネルを無効化
                if (!select_i2c_channel(fd, tca.address, -1, dummy)) {
                    std::cerr << "ERROR [clear]: Failed to disable all channels on TCA9548A 0x" << std::hex << tca.address << std::dec << std::endl;
                }
            }
        }
    }

    close(fd);
    return true;
#endif
}

