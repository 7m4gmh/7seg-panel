#include "common.h"
#include "audio.h"
#include "video.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <vector>
#include <cstring>

void udp_loop(int sockfd,int i2c_fd){
    char buf[65535];
    while(!finished){
        sockaddr_in cliaddr;
        socklen_t len=sizeof(cliaddr);
        ssize_t n=recvfrom(sockfd, buf,sizeof(buf),0,(sockaddr*)&cliaddr,&len);
        if(n<=0) continue;
        char type=buf[0];
        if(type=='S'){
            std::cout<<"[S] Start stream\n";
            start_time=(double)clock()/CLOCKS_PER_SEC;
            audio_bytes_received=0;
            audio_waiting=true;
            std::thread(video_thread,i2c_fd).detach();
        }else if(type=='A'){
            std::vector<char> chunk(buf+21, buf+n);
            {
                std::lock_guard<std::mutex> lock(audio_mtx);
                audio_buf.push_back(chunk);
                audio_bytes_received+=chunk.size();
            }
            if(audio_waiting && audio_buf.size()>(SAMPLE_RATE*CHANNELS*2*2)/AUDIO_CHUNK_SIZE){
                std::cout<<"[A] Start audio playback\n";
                std::thread(start_audio).detach();
                audio_waiting=false;
            }
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
        }
    }
}
