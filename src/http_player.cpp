// src/http_player.cpp
#include "common.h"
#include "playback.h"
#include "../cpp-httplib/httplib.h"

#include <iostream>
#include <vector>
#include <deque>
#include <thread>
#include <string>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <dirent.h>

// --- グローバル変数 ---
extern volatile sig_atomic_t g_should_exit;
std::deque<std::string> video_queue;
std::string g_currently_playing;
std::atomic<bool> g_stop_current_video(false);
std::mutex queue_mutex;
std::condition_variable queue_cond;
const size_t MAX_FILE_SIZE = 100 * 1024 * 1024;
std::vector<std::string> g_default_videos;
size_t g_next_default_video_index = 0;
const DisplayConfig* g_active_config = nullptr;

// --- 関数プロトタイプ ---
//void frame_to_grid(const cv::Mat& bw_frame, const DisplayConfig& config, std::vector<uint8_t>& grid);

// ★★★ 修正: ent.d_name を ent->d_name に ★★★
void load_default_videos(const std::string& path, std::vector<std::string>& videos) {
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(path.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string filename = ent->d_name; // ★★★ ここを修正 ★★★
            if (filename[0] != '.' && (
                filename.length() > 4 && (filename.substr(filename.length() - 4) == ".mp4" || filename.substr(filename.length() - 4) == ".mov")
            )) {
                videos.push_back(path + "/" + filename);
            }
        }
        closedir(dir);
        if (!videos.empty()) std::cout << videos.size() << " 個のデフォルト動画を '" << path << "' から読み込みました。" << std::endl;
        else std::cout << "'" << path << "' に再生可能なデフォルト動画が見つかりませんでした。" << std::endl;
    } else {
        std::cerr << "警告: デフォルト動画ディレクトリ '" << path << "' を開けませんでした。" << std::endl;
    }
}

void playback_thread_worker() {
    while (!g_should_exit) {
        std::string path_to_play;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cond.wait(lock, [&]{ return !video_queue.empty() || !g_default_videos.empty() || g_should_exit; });
            if (g_should_exit) break;
            if (!video_queue.empty()) {
                path_to_play = video_queue.front(); video_queue.pop_front();
            } else if (!g_default_videos.empty()) {
                path_to_play = g_default_videos[g_next_default_video_index];
                g_next_default_video_index = (g_next_default_video_index + 1) % g_default_videos.size();
            }
            if (!path_to_play.empty()) g_currently_playing = path_to_play;
        }

        if (!path_to_play.empty() && g_active_config != nullptr) {
            // ★★★ 変更: 新しい共通関数を呼び出す ★★★
            play_video_stream(path_to_play, *g_active_config, g_stop_current_video);
            
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                g_currently_playing = "";
            }
        }
    }
}

int main(int argc, char* argv[]) {
    setup_signal_handlers();

    std::string default_video_path = "default_videos";
    std::string config_name = "24x4";
    if (argc > 1) default_video_path = argv[1];
    if (argc > 2) config_name = argv[2];

    if (config_name == "12x8") g_active_config = &CONFIG_12x8_EXPANDED;
    else if (config_name == "48x4") g_active_config = &CONFIG_48x4_EXPANDED;
    else if (config_name == "24x8") g_active_config = &CONFIG_24x8_EXPANDED;
    else g_active_config = &CONFIG_24x4;
 

    std::cout << "Using display configuration: " << g_active_config->name << std::endl;
    std::cout << "Using default video directory: '" << default_video_path << "'" << std::endl;
    load_default_videos(default_video_path, g_default_videos);
    
    httplib::Server svr;
    const char* base_dir = "./www";
    if (!svr.set_mount_point("/", base_dir)) {
        std::cerr << "Error: The base directory '" << base_dir << "' does not exist." << std::endl;
        return 1;
    }
    
    // ★★★ 修正: 省略していたラムダ式を元に戻す ★★★
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
        if (req.form.has_field("index")) {
            try {
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
                res.status = 400;
                return;
            }
        }
        res.status = 200;
    });

    svr.Post("/stop", [&](const httplib::Request&, httplib::Response& res) {
        std::cout << "再生中止リクエストを受信。" << std::endl;
        g_stop_current_video = true;
        res.status = 200;
    });
    
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

