// src/udp.cpp
#include "common.h"
#include "video.h"
#include "audio.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <opencv2/opencv.hpp> // ★★★ cv::Mat を使うのでインクルードを追加 ★★★

void udp_loop(int sockfd, int i2c_fd, const DisplayConfig& config) {
    char buf[65535];

    if (!audio_init(SAMPLE_RATE, CHANNELS)) {
        std::cerr << "Audio init failed\n" << std::endl;
    }

    while (!finished) {
        sockaddr_in cliaddr{};
        socklen_t len = sizeof(cliaddr);
        ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0, (sockaddr *)&cliaddr, &len);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            else { perror("recvfrom"); break; }
        } else if (n == 0) {
            continue;
        }

        char type = buf[0];
        if (type == 'S') {
            std::cout << "[S] Start stream\n";
            start_time = (double)clock() / CLOCKS_PER_SEC;
            audio_bytes_received = 0;
            std::thread(video_thread, i2c_fd, std::ref(config)).detach();
        } else if (type == 'A') {
            std::vector<char> chunk(buf + 21, buf + n);
            audio_queue(chunk.data(), chunk.size());
        } else if (type == 'V') { // ★★★ このブロックを修正 ★★★
            int pts;
            memcpy(&pts, buf + 21, sizeof(int));
            std::vector<uint8_t> frame_vec(buf + 25, buf + n);
            {
                std::lock_guard<std::mutex> lock(frame_mtx);
                // latest_frame は std::vector<uint8_t> なので、
                // 受信したベクターをそのまま代入する
                if (!frame_vec.empty()) {
                    latest_frame = frame_vec;
                }
                last_pts_ms = pts;
            }
        } else if (type == 'E') { // ★★★ else if が正しく繋がるように修正 ★★★
            std::cout << "[E] End stream\n";
            finished = true;
            audio_cleanup();
        }
        // ループの最後はここに } が来るのが正しい
    }
}

