#include "common.h"
#include "led.h"
#include <thread>
#include <chrono>
#include <map>
#include "video.h"
#include <opencv2/opencv.hpp>

const std::map<int,std::pair<int,int>> SEG_MAP = {
    {0,{1,0}}, {1,{3,1}}, {2,{3,3}}, {3,{1,4}},
    {4,{0,3}}, {5,{0,1}}, {6,{1,2}}, {7,{4,4}}
};


// bw: 0 or 255のモノクロ画像 (W×Hサイズ)
// grid: TOTAL 要素 (7セグパターン)
void frame_to_grid(const cv::Mat& bw, std::vector<uint8_t>& grid) {
    for (int i = 0; i < TOTAL; i++) {
        int r = i / ACROSS;
        int c = i % ACROSS;
        int xs = c * (W / ACROSS);
        int ys = r * (H / DOWN);
        int seg = 0;
        for (auto &kv: SEG_MAP) {
            int bit = kv.first; auto [dx,dy] = kv.second;
            int px = xs + dx;
            int py = ys + dy;
            if (0 <= px && px < W && 0 <= py && py < H && bw.at<uint8_t>(py,px) > 128)
                seg |= (1 << bit);
        }
        grid[i] = seg;
    }
}

void video_thread(int i2c_fd) {
    std::vector<uint8_t> last(W*H,0);
    while(!finished) {
        {
            std::lock_guard<std::mutex> lock(frame_mtx);
            if (!latest_frame.empty()) {
                last = latest_frame;
            }
        }

        std::vector<uint8_t> grid(TOTAL,0);
        for(int i=0;i<TOTAL;i++){
            int r=i/ACROSS, c=i%ACROSS;
            int xs=c*(W/ACROSS), ys=r*(H/DOWN);
            int seg=0;
            for(auto &kv: SEG_MAP){
                int bit=kv.first; auto [dx,dy]=kv.second;
                int px=xs+dx, py=ys+dy;
                if(0<=px && px<W && 0<=py && py<H && last[py*W+px]>128)
                    seg|=(1<<bit);
            }
            grid[i]=seg;
        }
        update_display(i2c_fd, grid, module_addrs);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}
