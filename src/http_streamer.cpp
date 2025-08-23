#include "../cpp-httplib/httplib.h"
#include <gst/gst.h>
#include <glib.h>
#include <iostream>
#include <deque>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <csignal>
#include <fstream>
#include <sstream>
#include <unistd.h>

// --- GStreamer関連のグローバル変数 ---
GMainLoop *g_main_loop = nullptr;
GstElement *g_pipeline = nullptr;
std::mutex g_gst_mutex;

// --- アプリケーションのグローバル変数 ---
volatile sig_atomic_t g_should_exit = 0;
struct VideoInfo { std::string filename; std::string filepath; };
std::deque<VideoInfo> video_queue;
std::mutex queue_mutex;
std::condition_variable queue_cond;
std::string g_currently_streaming;
std::string g_youtube_stream_key;

// --- 設定ファイルの読み込み ---
bool load_stream_key() {
    std::ifstream key_file("stream_key.conf");
    if (!key_file.is_open()) {
        std::cerr << "致命的エラー: stream_key.conf が見つかりません。" << std::endl; return false;
    }
    std::getline(key_file, g_youtube_stream_key);
    if (g_youtube_stream_key.empty()) {
        std::cerr << "致命的エラー: stream_key.conf が空です。" << std::endl; return false;
    }
    std::cout << "ストリームキーをファイルから読み込みました。" << std::endl;
    return true;
}

// --- シグナルハンドラ ---
void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        if (!g_should_exit) { // 複数回呼ばれるのを防ぐ
            std::cout << "\n終了シグナルを捕捉。クリーンアップします..." << std::endl;
            g_should_exit = 1;
            std::lock_guard<std::mutex> gst_lock(g_gst_mutex);
            if (g_main_loop) g_main_loop_quit(g_main_loop);
        }
    }
}

void setup_signal_handlers() {
    struct sigaction action;
    action.sa_handler = signal_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}

// --- ストリーミング実行スレッド ---
void streaming_thread_worker() {
    while (!g_should_exit) {
        VideoInfo video_to_stream;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cond.wait(lock, [&]{ return !video_queue.empty() || g_should_exit; });
            if (g_should_exit) break;
            video_to_stream = video_queue.front();
            video_queue.pop_front();
        }

        {
            std::lock_guard<std::mutex> gst_lock(g_gst_mutex);
            if(g_should_exit) break; // ループの間に終了シグナルが来た場合
            g_currently_streaming = video_to_stream.filename;

            std::string rtmp_url = "rtmp://a.rtmp.youtube.com/live2/" + g_youtube_stream_key;
            std::string gst_pipeline_str = 
                "multifilesrc location=\"" + video_to_stream.filepath + "\" loop=true ! "
                "qtdemux name=demux ! flvmux name=mux ! rtmpsink location='" + rtmp_url + "' "
                "demux.video_0 ! queue ! mux.video "
                "demux.audio_0 ! queue ! mux.audio";

            std::cout << "ストリーミング開始: " << g_currently_streaming << std::endl;

            g_pipeline = gst_parse_launch(gst_pipeline_str.c_str(), NULL);
            if (!g_pipeline) {
                std::cerr << "パイプラインの作成に失敗しました。" << std::endl;
                g_currently_streaming = "";
                remove(video_to_stream.filepath.c_str());
                continue;
            }

            g_main_loop = g_main_loop_new(NULL, FALSE);
            gst_element_set_state(g_pipeline, GST_STATE_PLAYING);
        }
        
        g_main_loop_run(g_main_loop); // /stopが呼ばれるか、アプリが終了するまでここでブロック

        // --- クリーンアップ ---
        std::cout << "ストリーミングを停止しています..." << std::endl;
        {
            std::lock_guard<std::mutex> gst_lock(g_gst_mutex);
            if (g_pipeline) {
                gst_element_set_state(g_pipeline, GST_STATE_NULL);
                gst_object_unref(g_pipeline);
                g_pipeline = nullptr;
            }
            if (g_main_loop) {
                g_main_loop_unref(g_main_loop);
                g_main_loop = nullptr;
            }
            g_currently_streaming = "";
            remove(video_to_stream.filepath.c_str());
            std::cout << "ストリーミングは正常に停止しました。" << std::endl;
        }
    }
}

// --- メイン関数 ---
int main(int argc, char *argv[]) {
    gst_init(&argc, &argv); // GStreamerの初期化
    setup_signal_handlers();
    if (!load_stream_key()) return 1;

    httplib::Server svr;
    if (!svr.set_mount_point("/", "./www")) {
        std::cerr << "Error: The base directory './www' does not exist." << std::endl;
        return 1;
    }

    svr.Get("/status", [](const httplib::Request&, httplib::Response& res) {
        std::stringstream json;
        json << "{\"now_streaming\": \"" << g_currently_streaming << "\",\"queue\": [";
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            for (size_t i = 0; i < video_queue.size(); ++i) {
                json << "{\"index\": " << i << ", \"filename\": \"" << video_queue[i].filename << "\"}";
                if (i < video_queue.size() - 1) json << ",";
            }
        }
        json << "]}";
        res.set_content(json.str(), "application/json");
    });

    svr.Post("/upload", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.form.has_file("video_file")) {
            const auto& file = req.form.get_file("video_file");
            const std::string upload_path = "/tmp/" + file.filename;
            std::ofstream ofs(upload_path, std::ios::binary);
            if (!ofs) { res.status = 500; res.set_content("Failed to save file.", "text/plain"); return; }
            ofs.write(file.content.c_str(), file.content.length());
            ofs.close();
            { std::lock_guard<std::mutex> lock(queue_mutex); video_queue.push_back({file.filename, upload_path}); }
            queue_cond.notify_one();
            res.set_content("Upload successful.", "text/plain");
        } else { res.status = 400; res.set_content("No file found.", "text/plain"); }
    });

    svr.Post("/delete", [&](const httplib::Request& req, httplib::Response& res) { /* ... */ }); // 省略なしコードではここに実装

    svr.Post("/stop", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> gst_lock(g_gst_mutex);
        if (g_main_loop && g_main_loop_is_running(g_main_loop)) {
            g_main_loop_quit(g_main_loop); // GStreamerのメインループを安全に終了させる
        }
    });

    std::thread streaming_thread(streaming_thread_worker);
    std::thread server_thread([&]() {
        std::cout << "HTTPサーバーを http://<your-ip-address>:8080 で起動します" << std::endl;
        svr.listen("0.0.0.0", 8080);
    });

    // メインスレッドはここで終了シグナルを待つ
    while (!g_should_exit) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }

    std::cout << "\nサーバーを停止します..." << std::endl;
    svr.stop();
    queue_cond.notify_all(); // ストリーミングスレッドを起こして終了させる
    streaming_thread.join();
    server_thread.join();
    
    std::cout << "全ての処理が完了しました。" << std::endl;
    return 0;
}

