// src/server.cpp
#include "udp.h"
#include "common.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <csignal>

// ★★★ 変更: udp_loopのシグネチャ変更 ★★★
void udp_loop(int sockfd, int i2c_fd, const DisplayConfig& config);

int main(int argc, char* argv[]) {
    setup_signal_handlers();

    // ★★★ 変更: コマンドライン引数で設定を選択 ★★★
    std::string config_name = (argc > 1) ? argv[1] : "24x4";
    const DisplayConfig& active_config = (config_name == "12x8") ? CONFIG_12x8_EXPANDED : CONFIG_24x4;
    std::cout << "Using display configuration: " << active_config.name << std::endl;

    while (!g_should_exit) {
        int i2c_fd = open("/dev/i2c-0", O_RDWR);
        if (i2c_fd < 0) { perror("open /dev/i2c-0"); return 1; }

        // ★★★ 変更: 初期化処理をここで行う ★★★
        if (!initialize_displays(i2c_fd, active_config)) {
             std::cerr << "Failed to initialize displays." << std::endl;
             close(i2c_fd);
             return 1;
        }

        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        struct timeval tv{1, 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        sockaddr_in servaddr{};
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = INADDR_ANY;
        servaddr.sin_port = htons(PORT);

        if (bind(sockfd, (sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
            perror("bind"); return 1;
        }

        // ★★★ 変更: configを渡す ★★★
        udp_loop(sockfd, i2c_fd, active_config);

        close(sockfd);
        close(i2c_fd);
    }
    return 0;
}

