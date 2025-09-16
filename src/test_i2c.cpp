// src/test_i2c.cpp
#include "common.h"
#include "config_loader.hpp"
#include "led.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

static int try_open_i2c_auto() {
    const char* candidates[] = {"/dev/i2c-0", "/dev/i2c-1"};
    for (auto dev : candidates) {
        int fd = open(dev, O_RDWR);
        if (fd >= 0) {
            std::cout << "Opened I2C device: " << dev << std::endl;
            return fd;
        } else {
            perror(dev);
        }
    }
    return -1;
}

int main(int argc, char* argv[]) {
    std::string cfg = (argc > 1) ? argv[1] : "24x4";
    DisplayConfig dc;
    try {
        dc = load_config_from_json(cfg);
        std::cout << "Config: " << dc.name << " (TCA addresses: ";
        for (const auto& [bus_id, bus_config] : dc.buses) {
            for (const auto& tca : bus_config.tca9548as) {
                if (tca.address != -1) {
                    std::cout << "0x" << std::hex << tca.address << std::dec << " ";
                }
            }
        }
        std::cout << ")\n";
        std::cout << "Addresses: ";
        auto addrs = dc.all_addresses();
        for (size_t i = 0; i < addrs.size(); ++i) {
            std::cout << "0x" << std::hex << addrs[i] << std::dec << (i+1<addrs.size()?", ":"\n");
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        return 1;
    }

    int i2c_fd = try_open_i2c_auto();
    if (i2c_fd < 0) {
        std::cerr << "ERROR: Failed to open any I2C device (/dev/i2c-0 or /dev/i2c-1)." << std::endl;
        return 1;
    }

    if (!initialize_displays(i2c_fd, dc)) {
        std::cerr << "ERROR: initialize_displays failed. Check TCA address and module wiring." << std::endl;
        close(i2c_fd);
        return 2;
    }

    std::cout << "Writing ALL-ON pattern..." << std::endl;
    std::vector<uint8_t> grid_on(dc.total_digits(), 0xFF);
    I2CErrorInfo err;
    if (!update_flexible_display(i2c_fd, dc, grid_on, err)) {
        std::cerr << "ERROR: update_flexible_display (ALL-ON) failed at ch=" << err.channel
                  << " addr=0x" << std::hex << err.address << std::dec << std::endl;
        close(i2c_fd);
        return 3;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    std::cout << "Clearing (ALL-OFF)..." << std::endl;
    std::vector<uint8_t> grid_off(dc.total_digits(), 0x00);
    if (!update_flexible_display(i2c_fd, dc, grid_off, err)) {
        std::cerr << "ERROR: update_flexible_display (ALL-OFF) failed at ch=" << err.channel
                  << " addr=0x" << std::hex << err.address << std::dec << std::endl;
        close(i2c_fd);
        return 4;
    }

    std::cout << "Done." << std::endl;
    close(i2c_fd);
    return 0;
}
