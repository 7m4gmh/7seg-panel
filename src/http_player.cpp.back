// http_player.cpp (バックエンドのバグを修正した最終版)
#include "common.h"
#include "led.h"
#include "video.h"
#include "../cpp-httplib/httplib.h"

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>

extern volatile sig_atomic_t g_should_exit;
//extern std::vector<int> module_addrs;
void frame_to_grid(const cv::Mat& bw_frame, std::vector<uint8_t>& grid);
void update_display(int i2c_fd, const std::vector<uint8_t>& grid, const std::vector<int>& addrs);
std::deque<std::string> video_queue;
std::string g_currently_playing;
std::atomic<bool> g_stop_current_video(false);
std::mutex queue_mutex;
std::condition_variable queue_cond;
const size_t MAX_FILE_SIZE = 100 * 1024 * 1024;

int play_video(const std::string& video_path) {
    g_stop_current_video = false;
    int i2c_fd = open("/dev/i2c-0", O_RDWR);
    if (i2c_fd < 0) { perror("open /dev/i2c-0 に失敗"); return 1; }
    if (!initialize_displays(i2c_fd, MODULE_ADDRESSES)) {
        std::cerr << "Failed to initialize display modules." << std::endl;
        close(i2c_fd);
        return 1;
    }

    cv::VideoCapture cap(video_path, cv::CAP_FFMPEG);
    if (!cap.isOpened()) { std::cerr << "動画ファイルを開けません: " << video_path << std::endl; close(i2c_fd); return 1; }
    std::string command = "ffplay -nodisp -autoexit \"" + video_path + "\" > /dev/null 2>&1 &";
    system(command.c_str());
    double fps = cap.get(cv::CAP_PROP_FPS);
    if (fps <= 0) fps = 30.0;
    auto frame_duration = std::chrono::microseconds(static_cast<long long>(1000000.0 / fps));
    std::cout << "再生開始: " << video_path << " (" << fps << " FPS)" << std::endl;
    auto next_frame_time = std::chrono::steady_clock::now();
    cv::Mat frame;
    while (!g_should_exit && !g_stop_current_video && cap.read(frame)) {
        cv::Mat cropped_frame;
        float source_aspect = (float)frame.cols / frame.rows;
        const float target_aspect = (float)W / H;
        if (source_aspect > target_aspect) {
            int new_width = static_cast<int>(frame.rows * target_aspect); int x = (frame.cols - new_width) / 2;
            cv::Rect crop_region(x, 0, new_width, frame.rows); cropped_frame = frame(crop_region);
        } else {
            int new_height = static_cast<int>(frame.cols / target_aspect); int y = (frame.rows - new_height) / 2;
            cv::Rect crop_region(0, y, frame.cols, new_height); cropped_frame = frame(crop_region);
        }
        cv::Mat resized_frame, gray_frame, bw_frame;
        cv::resize(cropped_frame, resized_frame, cv::Size(W, H));
        cv::cvtColor(resized_frame, gray_frame, cv::COLOR_BGR2GRAY);
        cv::threshold(gray_frame, bw_frame, 128, 255, cv::THRESH_BINARY);
        std::vector<uint8_t> grid(TOTAL, 0);
        frame_to_grid(bw_frame, grid);
        update_display(i2c_fd, grid, MODULE_ADDRESSES);
        next_frame_time += frame_duration;
        std::this_thread::sleep_until(next_frame_time);
    }
    system("killall ffplay > /dev/null 2>&1");
    cap.release();
    close(i2c_fd);
    if (g_stop_current_video) { std::cout << "再生中止: " << video_path << std::endl; }
    else { std::cout << "再生終了: " << video_path << std::endl; }
    return 0;
}
void playback_thread_worker() {
    while (!g_should_exit) {
        std::string path_to_play;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cond.wait(lock, [&]{ return !video_queue.empty() || g_should_exit; });
            if (g_should_exit) break;
            path_to_play = video_queue.front();
            video_queue.pop_front();
            g_currently_playing = path_to_play;
        }
        play_video(path_to_play);
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            g_currently_playing = "";
        }
    }
}

int main(void) {
    setup_signal_handlers();
    httplib::Server svr;

    const char* base_dir = "./www";
    if (!svr.set_mount_point("/", base_dir)) {
        std::cerr << "Error: The base directory '" << base_dir << "' does not exist." << std::endl;
        return 1;
    }

    // APIエンドポイント: /status (変更なし)
    svr.Get("/status", [](const httplib::Request&, httplib::Response& res) {
        std::stringstream json;
        json << "{";
        json << "\"now_playing\": \"" << (g_currently_playing.empty() ? "(none)" : g_currently_playing.substr(g_currently_playing.find_last_of('/') + 1)) << "\",";
        json << "\"queue\": [";
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            for (size_t i = 0; i < video_queue.size(); ++i) {
                const auto& path = video_queue[i];
                json << "{\"index\": " << i << ", \"filename\": \"" << path.substr(path.find_last_of('/') + 1) << "\"}";
                if (i < video_queue.size() - 1) json << ",";
            }
        }
        json << "]}";
        res.set_content(json.str(), "application/json");
    });
    
    // APIエンドポイント: /upload (変更なし)
    svr.Post("/upload", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.form.has_file("video_file")) {
            const auto& file = req.form.get_file("video_file");
            if (file.content.length() > MAX_FILE_SIZE) { res.status = 413; res.set_content("File size exceeds limit.", "text/plain"); return; }
            const std::string upload_path = "/tmp/" + file.filename;
            std::ofstream ofs(upload_path, std::ios::binary); ofs.write(file.content.c_str(), file.content.length()); ofs.close();
            std::cout << "ファイルを受信し、キューに追加: " << file.filename << std::endl;
            { std::lock_guard<std::mutex> lock(queue_mutex); video_queue.push_back(upload_path); }
            queue_cond.notify_one();
            res.status = 200; res.set_content("Upload successful.", "text/plain");
        } else {
            res.set_content("ファイルが見つかりません。", "text/plain"); res.status = 400;
        }
    });

    svr.Post("/delete", [&](const httplib::Request& req, httplib::Response& res) {
        // multipart/form-data のフィールドを正しく読み取るように修正
        if (req.form.has_field("index")) {
            try {
                // get_field で値を取得
                size_t index = std::stoul(req.form.get_field("index"));
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    if (index < video_queue.size()) {
                        std::cout << "キューから削除: " << video_queue[index] << std::endl;
                        video_queue.erase(video_queue.begin() + index);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Invalid index received for deletion: " << e.what() << std::endl;
                res.status = 400; // Bad Request
                return;
            }
        }
        res.status = 200;
    });

    // APIエンドポイント: /stop (変更なし、もともと正しく動作するはず)
    svr.Post("/stop", [&](const httplib::Request&, httplib::Response& res) {
        std::cout << "再生中止リクエストを受信。" << std::endl;
        g_stop_current_video = true;
        res.status = 200;
    });
    
    // (スレッド起動と終了処理は変更なし)
    std::cout << "再生キュー処理スレッドを起動します..." << std::endl;
    std::thread playback_thread(playback_thread_worker);
    std::thread server_thread([&]() {
        std::cout << "HTTPサーバーを http://<your-ip-address>:8080 で起動します" << std::endl;
        svr.listen("0.0.0.0", 8080);
    });
    while (!g_should_exit) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }
    std::cout << "\nサーバーを停止します..." << std::endl;
    svr.stop();
    g_stop_current_video = true;
    queue_cond.notify_all();
    server_thread.join();
    playback_thread.join();
    system("killall ffplay > /dev/null 2>&1");
    std::cout << "全ての処理が完了しました。" << std::endl;
    return 0;
}

