// src/udp.cpp (最終形)

#include "udp.h"
#include "video.h"
#include "audio.h"
#include "common.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>

void udp_loop(int i2c_fd, int sockfd, const DisplayConfig& config, std::atomic<bool>& stop_flag) {
    char buf[65535];

    if (!audio_init(SAMPLE_RATE, CHANNELS)) {
        std::cerr << "Audio init failed" << std::endl;
    }

    while (!stop_flag) {
        sockaddr_in cliaddr{};
        socklen_t len = sizeof(cliaddr);
        
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

        ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0, (sockaddr *)&cliaddr, &len);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("recvfrom");
            break;
        }
        if (n == 0) continue;

        char type = buf[0];
        if (type == 'S') {
            std::cout << "[S] Stream Start" << std::endl;
            start_time = (double)clock() / CLOCKS_PER_SEC;
            audio_bytes_received = 0;
            // video.h と引数を完全に一致させます
#ifndef __APPLE__
            std::thread(video_thread, std::ref(i2c_fd), std::ref(config), std::ref(stop_flag)).detach();
#endif
        } else if (type == 'A') {
            std::vector<char> chunk(buf + 21, buf + n);
            audio_queue(chunk.data(), chunk.size());
        } else if (type == 'V') {
            int pts;
            memcpy(&pts, buf + 21, sizeof(int));
            std::vector<uint8_t> frame_vec(buf + 25, buf + n);
            {
                std::lock_guard<std::mutex> lock(frame_mtx);
                if (!frame_vec.empty()) {
                    latest_frame = frame_vec;
                }
                last_pts_ms = pts;
            }
        } else if (type == 'E') {
            std::cout << "[E] Stream End" << std::endl;
            stop_flag = true; 
        }
    }
    audio_cleanup();
}

void start_udp_server(int i2c_fd, int port, const DisplayConfig& config, std::atomic<bool>& stop_flag) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return;
    }

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return;
    }

    udp_loop(i2c_fd, sockfd, config, stop_flag);
    close(sockfd);
}