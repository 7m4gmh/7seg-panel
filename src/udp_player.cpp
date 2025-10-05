// src/udp_player.cpp (今度こそ、本当に、最終形です)

#include "common.h"
#include "config.h"
#include "main_common.hpp"
#include "udp.h"
#include "led.h"
#include "config_loader.hpp" // ★★★ JSONを読み込むために必須 ★★★
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [config_name] [options]" << std::endl;
        std::cerr << "  config_name from config.json (e.g., 24x4)" << std::endl;
        std::cerr << "  options:" << std::endl;
        std::cerr << "    --crop, -c: Crop to fit aspect ratio" << std::endl;
        std::cerr << "    --stretch, -s: Stretch to fill display" << std::endl;
        std::cerr << "    --fit, -f: Fit entire video within display (may add padding) (default)" << std::endl;
        std::cerr << "    --threshold min max, -t min max: Set binarization threshold (default: 64 255)" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    std::string config_name = "24x4";
    ScalingMode scaling_mode = ScalingMode::CROP;
    int min_threshold = 64;
    int max_threshold = 255;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--stretch" || arg == "-s") {
            scaling_mode = ScalingMode::STRETCH;
        } else if (arg == "--crop" || arg == "-c") {
            scaling_mode = ScalingMode::CROP;
        } else if (arg == "--threshold" || arg == "-t") {
            if (i + 2 < argc) {
                try {
                    min_threshold = std::stoi(argv[i + 1]);
                    max_threshold = std::stoi(argv[i + 2]);
                    i += 2;
                } catch (const std::exception&) {
                    std::cerr << "Invalid threshold values. Expected integers." << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Missing threshold values after " << arg << std::endl;
                return 1;
            }
        } else {
            config_name = arg;
        }
    }

    DisplayConfig active_config;
    try {
        active_config = load_config_from_json(config_name);
        std::cout << "Successfully loaded configuration: " << active_config.name << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return 1;
    }
    
    int i2c_fd = open("/dev/i2c-0", O_RDWR);
    if (i2c_fd < 0) {
        perror("Failed to open /dev/i2c-0");
        return 1;
    }

    if (!initialize_displays(i2c_fd, active_config)) {
        std::cerr << "Failed to initialize display modules." << std::endl;
        close(i2c_fd);
        return 1;
    }

    setup_signal_handlers(); 

    std::atomic<bool> stop_flag(false);
    std::thread watch_dog([&stop_flag] {
        while(!g_should_exit) {
             std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        stop_flag = true;
    });

    start_udp_server(i2c_fd, port, active_config, stop_flag);
    
    close(i2c_fd);
    
    if(watch_dog.joinable()) {
        watch_dog.join();
    }

    std::cout << "\nプログラムを終了します。" << std::endl;
    return 0;
}