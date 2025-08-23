#include "common.h"
#include "video.h"
#include "audio.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>

void udp_loop(int sockfd, int i2c_fd){
    char buf[65535];

    // SDLオーディオ初期化 (44100Hz, ステレオ)
    if (!audio_init(44100, 2)) {
        std::cerr << "Audio init failed\n" << std::endl;
    }

    while(!finished){
	        sockaddr_in cliaddr{};
    socklen_t len = sizeof(cliaddr);
    ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0, (sockaddr *)&cliaddr, &len);

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // タイムアウト: ループを続け、finished をチェック
            continue;
        } else {
            perror("recvfrom");
            break; // 致命的なエラー
        }
    } else if (n == 0) {
        // 0バイトパケットを受信したケース → 特に処理せずスキップ
        continue;
    }
    // n > 0 の場合：受け取ったパケットを処理
        char type=buf[0];
        if(type=='S'){
            std::cout<<"[S] Start stream\n";
            start_time=(double)clock()/CLOCKS_PER_SEC;
            audio_bytes_received=0;
            std::thread(video_thread,i2c_fd).detach();
        }else if(type=='A'){
		std::vector<char> chunk(buf+21, buf+n);
    		audio_queue(chunk.data(), chunk.size());
        }else if(type=='V'){
            int pts; memcpy(&pts, buf+21,sizeof(int));
            std::vector<uint8_t> frame(buf+25, buf+n);
            {
                std::lock_guard<std::mutex> lock(frame_mtx);
                latest_frame=frame;
                last_pts_ms=pts;
            }
        }else if(type=='E'){
            std::cout<<"[E] End stream\n";
            finished=true;
            audio_cleanup();
        }
    }
}

