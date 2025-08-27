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

bool initialize_displays(int i2c_fd, const DisplayConfig& config)
{
    std::cout << "Initializing modules..." << std::endl;
    std::vector<int> module_addrs = config.all_addresses();
    for (int addr : module_addrs) {
        if (ioctl(i2c_fd, I2C_SLAVE, addr) < 0) {
            perror("ioctl I2C_SLAVE failed during initialization");
            return false;
        }
        uint8_t commands[] = { 0x21, 0x81, 0xEF };
        for (uint8_t cmd : commands) {
            if (write(i2c_fd, &cmd, 1) != 1) {
                fprintf(stderr, "Failed to write command 0x%02X to address 0x%02X\n", cmd, addr);
                perror("i2c write command");
            }
            usleep(1000);
        }
    }
    std::cout << "Initialization complete for " << module_addrs.size() << " modules." << std::endl;
    return true;
}

void update_module_from_grid(int i2c_bus_fd, int addr, const std::vector<uint8_t>& grid16)
{
    uint8_t display_buffer[16] = {0};
    const int DIGITS_PER_MODULE = 16;

    // ★★★ 修正: キャストを適用 ★★★
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
        perror("i2c write");
    }
}

void update_flexible_display(int i2c_fd, const DisplayConfig& config, const std::vector<uint8_t>& grid)
{
    const int TOTAL_WIDTH = config.total_width;
    const int MOD_DIGITS_W = config.module_digits_width;
    const int MOD_DIGITS_H = config.module_digits_height;
    const int DIGITS_PER_MODULE = MOD_DIGITS_W * MOD_DIGITS_H;

    std::map<int, std::vector<uint8_t>> modules_data;
    for (int addr : config.all_addresses()) {
        modules_data[addr].resize(DIGITS_PER_MODULE, 0);
    }

    // ★★★ 修正: キャストを適用 ★★★
    for (int i = 0; static_cast<size_t>(i) < grid.size() && i < config.total_digits(); ++i) {
        int total_row = i / TOTAL_WIDTH;
        int total_col = i % TOTAL_WIDTH;

        int module_grid_row = total_row / MOD_DIGITS_H;
        int module_grid_col = total_col / MOD_DIGITS_W;
        
        // ★★★ 修正: 両方の比較にキャストを適用 ★★★
        if (static_cast<size_t>(module_grid_row) >= config.module_grid.size() ||
            (static_cast<size_t>(module_grid_row) < config.module_grid.size() && static_cast<size_t>(module_grid_col) >= config.module_grid[module_grid_row].size()) ) {
            continue;
        }
        int target_addr = config.module_grid[module_grid_row][module_grid_col];

        int row_in_module = total_row % MOD_DIGITS_H;
        int col_in_module = total_col % MOD_DIGITS_W;
        int digit_index_on_module = (row_in_module * MOD_DIGITS_W) + col_in_module;
        
        if (digit_index_on_module < DIGITS_PER_MODULE) {
            modules_data[target_addr][digit_index_on_module] = grid[i];
        }
    }

    for (auto const& [addr, data] : modules_data) {
        update_module_from_grid(i2c_fd, addr, data);
    }
}


