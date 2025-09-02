#include "common.h"
#include "playback.h"
#include "main_common.hpp" // ★★★ 共通ヘッダーをインクルード
#include "../cpp-httplib/httplib.h"

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

#include <csignal>   // ★★★ signal のために追加 ★★★
#include <map>       // ★★★ エラー分析のために追加 ★★★
#include <utility>   // ★★★ エラー分析のために追加 ★★★
#include <iomanip>   // ★★★ std::hex のために追加 ★★★

// --- グローバル変数 ---
std::deque<std::string> video_queue;
std::string g_currently_playing;
std::atomic<bool> g_stop_current_video(false);
std::mutex queue_mutex;
std::condition_variable queue_cond;
const size_t MAX_FILE_SIZE = 100 * 1024 * 1024;
std::vector<std::string> g_default_videos;
size_t g_next_default_video_index = 0;

// I2Cエラー分析用のグローバル変数
std::map<std::pair<int, int>, int> g_error_counts;

// アプリケーション終了用のシグナルハンドラ
void http_player_shutdown_handler(int signal_num) {
    if (signal_num == SIGINT) {
        if (!g_should_exit) {
            std::cout << "\nSIGINT received, shutting down..." << std::endl;
            
            // --- I2C Error Analysis ---
            std::cout << "\n--- I2C Error Analysis ---" << std::endl;
            if (g_error_counts.empty()) {
                std::cout << "No I2C errors were recorded." << std::endl;
            } else {
                for (const auto& [key, count] : g_error_counts) {
                    std::cout << "Channel: " << key.first 
                              << ", Address: 0x" << std::hex << key.second << std::dec
                              << "  => " << count << " errors" << std::endl;
                }
            }
            std::cout << "--------------------------" << std::endl;

            g_should_exit = true;
        }
    }
}


void load_default_videos(const std::string& path, std::vector<std::string>& videos) {
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(path.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            std::string filename = ent->d_name;
            if (filename[0] != '.' && (
                filename.length() > 4 && (filename.substr(filename.length() - 4) == ".mp4" || filename.substr(filename.length() - 4) == ".mov")
            )) {
                videos.push_back(path + "/" + filename);
            }
        }
        closedir(dir);
        if (!videos.empty()) std::cout << videos.size() << " default videos loaded from '" << path << "'." << std::endl;
        else std::cout << "No default videos found in '" << path << "'." << std::endl;
    } else {
        std::cerr << "Warning: Could not open default video directory '" << path << "'." << std::endl;
    }
}

void playback_thread_worker(const DisplayConfig& config) { // ★★★ configを引数で受け取る
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

        if (!path_to_play.empty()) {
            play_video_stream(path_to_play, config, g_stop_current_video);
            g_stop_current_video = false; // 次の再生のためにリセット
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                g_currently_playing = "";
            }
        }
    }
}

int main(int argc, char* argv[]) {
    const std::string usage = 
        "Usage: " + std::string(argv[0]) + " <default_video_path> [config_name]\n"
        "  config_name: 24x4 (default), 12x8, etc. from config.json";

    signal(SIGINT, http_player_shutdown_handler);

    return common_main_runner(usage, argc, argv,
        [](const std::string& default_video_path, const DisplayConfig& config) {
            std::cout << "Using default video directory: '" << default_video_path << "'" << std::endl;
            load_default_videos(default_video_path, g_default_videos);
            
            httplib::Server svr;
            const char* base_dir = "./www";
            if (!svr.set_mount_point("/", base_dir)) {
                std::cerr << "Error: The base directory '" << base_dir << "' does not exist." << std::endl;
                return;
            }
            
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
                    std::cout << "File received and queued: " << file.filename << std::endl;
                    { std::lock_guard<std::mutex> lock(queue_mutex); video_queue.push_back(upload_path); }
                    queue_cond.notify_one();
                    res.status = 200; res.set_content("Upload successful.", "text/plain");
                } else {
                    res.set_content("File not found.", "text/plain"); res.status = 400;
                }
            });

            svr.Post("/delete", [&](const httplib::Request& req, httplib::Response& res) {
                if (req.form.has_field("index")) {
                    try {
                        size_t index = std::stoul(req.form.get_field("index"));
                        {
                            std::lock_guard<std::mutex> lock(queue_mutex);
                            if (index < video_queue.size()) {
                                std::cout << "Removing from queue: " << video_queue[index] << std::endl;
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
                std::cout << "Stop request received." << std::endl;
                g_stop_current_video = true;
                res.status = 200;
            });
            // --- ▲▲▲ ここまで ▲▲▲ ---

            std::cout << "Starting playback queue thread..." << std::endl;
            std::thread playback_thread(playback_thread_worker, std::ref(config));

            std::thread server_thread([&]() {
                std::cout << "HTTP server starting at http://<your-ip-address>:8080" << std::endl;
                svr.listen("0.0.0.0", 8080);
            });
            
            while (!g_should_exit) { 
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
            }
            
            std::cout << "\nStopping server..." << std::endl;
            svr.stop();
            g_stop_current_video = true;
            queue_cond.notify_all();
            
            if (server_thread.joinable()) server_thread.join();
            if (playback_thread.joinable()) playback_thread.join();

            system("killall ffplay > /dev/null 2>&1");
            std::cout << "All processes have been completed." << std::endl;
        }
    );
}