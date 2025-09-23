#include "common.h"
#include "config.h"
#include "config_loader.hpp"
#include "led.h"
#include "video.h"
#include "audio.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <opencv2/opencv.hpp>

#include <atomic>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

// common.h で宣言されているグローバル変数と関数を使用します
// static std::atomic<bool> stop_flag(false);
// static std::atomic<bool> g_should_exit(false);
// static void setup_signal_handlers();

// 受信開始判定などの追加フラグは使わない（従来動作に戻す）

static GstFlowReturn on_new_sample_video(GstAppSink* sink, gpointer user_data) {
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    if (!buffer || !caps) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstVideoInfo vinfo;
    if (!gst_video_info_from_caps(&vinfo, caps)) {
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    int width = GST_VIDEO_INFO_WIDTH(&vinfo);
    int height = GST_VIDEO_INFO_HEIGHT(&vinfo);
    int stride = GST_VIDEO_INFO_PLANE_STRIDE(&vinfo, 0);
    cv::Mat img(height, width, CV_8UC3, (void*)map.data, stride);

    std::vector<uint8_t> jpg;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 45};
    cv::imencode(".jpg", img, jpg, params);

    GstClockTime pts = GST_BUFFER_PTS(buffer);
    int pts_ms = 0;
    if (GST_CLOCK_TIME_IS_VALID(pts)) {
        pts_ms = static_cast<int>(pts / GST_MSECOND);
    }

    {
        std::lock_guard<std::mutex> lk(frame_mtx);
        if (!jpg.empty()) {
            latest_frame = std::move(jpg);
        }
        last_pts_ms = pts_ms;
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

static GstFlowReturn on_new_sample_audio(GstAppSink* sink, gpointer user_data) {
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    static int warmup_bytes = []{
        int ms = 100; if (const char* v = std::getenv("AUDIO_WARMUP_MS")) ms = std::max(0, atoi(v));
        return (48000 * 2 * 2 * ms) / 1000;
    }();
    if ((int)map.size > 0) {
        if (warmup_bytes > 0) {
            int drop = std::min<int>(warmup_bytes, (int)map.size);
            warmup_bytes -= drop;
            if ((int)map.size > drop) {
                audio_queue(reinterpret_cast<const char*>(map.data) + drop, static_cast<int>(map.size) - drop);
            }
        } else {
            audio_queue(reinterpret_cast<const char*>(map.data), static_cast<int>(map.size));
        }
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

struct BusCtx {
    GMainLoop* loop {nullptr};
    GstElement* pipeline {nullptr};
    bool auto_restart {false}; // flv/flv_srvモード時にEOS/ERRORでパイプライン再生成
    bool request_restart {false};
};

// EOS/ERRORで即終了するか（デフォルト: 有効）。環境変数 EXIT_ON_EOF=0/false/off で無効化可。
static bool g_exit_on_eof = true;

static gboolean on_bus_message(GstBus* bus, GstMessage* msg, gpointer user_ptr) {
    BusCtx* ctx = reinterpret_cast<BusCtx*>(user_ptr);
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar* dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            std::cerr << "[GStreamer][ERROR] " << (err ? err->message : "unknown") << std::endl;
            if (dbg) {
                std::cerr << "  debug: " << dbg << std::endl;
                g_free(dbg);
            }
            if (err) g_error_free(err);
            if (ctx) {
                if (g_exit_on_eof) {
                    std::cout << "[GStreamer] ERROR -> exit (EXIT_ON_EOF=1)" << std::endl;
                    g_should_exit = true;
                } else if (ctx->auto_restart) {
                    ctx->request_restart = true;
                }
                if (ctx->loop) g_main_loop_quit(ctx->loop);
            }
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "[GStreamer] EOS" << std::endl;
            if (ctx) {
                if (g_exit_on_eof) {
                    std::cout << "[GStreamer] EOS -> exit (EXIT_ON_EOF=1)" << std::endl;
                    g_should_exit = true;
                } else if (ctx->auto_restart) {
                    ctx->request_restart = true;
                }
                if (ctx->loop) g_main_loop_quit(ctx->loop);
            }
            break;
        default:
            break;
    }
    return TRUE;
}


int main(int argc, char* argv[]) {
    // 使い方:
    // ./7seg-net-player [mode] [config_name] [port]
    // mode: ts (デフォルト/MPEG-TS), flv (FLV over TCP), flv_srv (TCP accept + fdsrc), raw (rawvideo/gray), stdin (標準入力raw)
    // 例: ./7seg-net-player ts 16x16_expanded 5004
    //     ./7seg-net-player flv 24x4 5004
    //     ./7seg-net-player raw 24x4 5004
    //     ./7seg-net-player stdin 24x4

    std::string mode = (argc > 1) ? argv[1] : "ts";
    std::string config_name = (argc > 2) ? argv[2] : "24x4";
    int port = (argc > 3) ? std::stoi(argv[3]) : 5004;

    DisplayConfig active_config;
    try {
        active_config = load_config_from_json(config_name);
        std::cout << "Successfully loaded configuration: " << active_config.name << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return 1;
    }

    int i2c_fd = open_i2c_auto(active_config);
    if (i2c_fd < 0) {
        perror("Failed to open I2C device");
        return 1;
    }
    if (!initialize_displays(i2c_fd, active_config)) {
        std::cerr << "Failed to initialize display modules." << std::endl;
        close(i2c_fd);
        return 1;
    }

    setup_signal_handlers();

    if (!audio_init(SAMPLE_RATE, CHANNELS)) {
        std::cerr << "Audio init failed" << std::endl;
    }

    std::thread vthr(video_thread, std::ref(i2c_fd), std::ref(active_config), std::ref(g_should_exit));

    gst_init(&argc, &argv);

    // 環境変数でEOS/ERROR時の終了を制御（0/false/offで無効化、1/true/onで有効化）
    if (const char* ex = std::getenv("EXIT_ON_EOF")) {
        if (strcasecmp(ex, "0") == 0 || strcasecmp(ex, "false") == 0 || strcasecmp(ex, "off") == 0) {
            g_exit_on_eof = false;
        } else if (strcasecmp(ex, "1") == 0 || strcasecmp(ex, "true") == 0 || strcasecmp(ex, "on") == 0) {
            g_exit_on_eof = true;
        }
    }

    // 追加の終了制御は行わず、従来の自動再始動に戻す

    auto has_element = [](const char* name) -> bool {
        GstElementFactory* f = gst_element_factory_find(name);
        if (f) { gst_object_unref(f); return true; }
        return false;
    };
    const bool have_avdec_h264 = has_element("avdec_h264");
    const bool have_avdec_aac  = has_element("avdec_aac");

    std::string pipeline_desc;
    if (mode == "ts") {
        pipeline_desc =
            "udpsrc port=" + std::to_string(port) + " caps=\"video/mpegts,systemstream=true,packetsize=188\" buffer-size=2097152 do-timestamp=true ! "
            "queue2 use-buffering=true max-size-time=1500000000 max-size-buffers=0 max-size-bytes=0 ! "
            "tsparse set-timestamps=true ! tsdemux name=demux "
            // 映像
            "demux. ! queue2 use-buffering=true max-size-time=1000000000 max-size-buffers=0 max-size-bytes=0 ! "
            "decodebin ! videoconvert ! video/x-raw,format=BGR ! "
            "appsink name=vsink emit-signals=true sync=false max-buffers=8 drop=true "
            // 音声
            "demux. ! queue2 use-buffering=true max-size-time=1500000000 max-size-buffers=0 max-size-bytes=0 ! "
            "decodebin ! audioconvert ! audioresample ! audio/x-raw,format=S16LE,rate=" + std::to_string(SAMPLE_RATE) + ",channels=" + std::to_string(CHANNELS) + " ! "
            "appsink name=asink emit-signals=true sync=false max-buffers=100 drop=false";
    } else if (mode == "flv") {
        // 以前の安定版: tcpserversrc → typefind → decodebin（映像/音声のPadに自動分岐）
        pipeline_desc =
            "tcpserversrc host=0.0.0.0 port=" + std::to_string(port) + " ! "
            "queue2 use-buffering=true max-size-time=2000000000 max-size-buffers=0 max-size-bytes=0 ! "
            "typefind ! decodebin name=dec "
            // 映像
            "dec. ! queue leaky=2 max-size-buffers=8 max-size-bytes=0 max-size-time=0 ! "
            "videoconvert ! video/x-raw,format=BGR ! "
            "appsink name=vsink emit-signals=true sync=false max-buffers=8 drop=true "
            // 音声
            "dec. ! queue max-size-buffers=100 max-size-bytes=0 max-size-time=0 ! "
            "audioconvert ! audioresample ! audio/x-raw,format=S16LE,rate=" + std::to_string(SAMPLE_RATE) + ",channels=" + std::to_string(CHANNELS) + " ! "
            "appsink name=asink emit-signals=true sync=false max-buffers=100 drop=false";
    } else if (mode == "flv_srv") {
        // 自前のTCPサーバで accept -> fdsrc でGStreamerへ供給（tcpserversrcの再接続不安定を回避）
        int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0) { perror("socket"); return 1; }
        int yes = 1;
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        ::setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY); addr.sin_port = htons(port);
        if (::bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); ::close(listen_fd); return 1; }
    if (::listen(listen_fd, 1) < 0) { perror("listen"); ::close(listen_fd); return 1; }
    // accept がブロックしすぎるのを防ぐ（停止要求に素早く応答）
    struct timeval tv; tv.tv_sec = 1; tv.tv_usec = 0;
    ::setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::cout << "[flv_srv] Listening on 0.0.0.0:" << port << std::endl;

        auto build_and_run_from_fd = [&](int conn_fd) -> bool {
            std::string pdesc =
                "fdsrc fd=" + std::to_string(conn_fd) + " do-timestamp=true blocksize=4096 ! "
                "queue2 use-buffering=true max-size-time=2000000000 max-size-buffers=0 max-size-bytes=0 ! "
                "typefind ! decodebin name=dec "
                // 映像
                "dec. ! queue leaky=2 max-size-buffers=8 max-size-bytes=0 max-size-time=0 ! "
                "videoconvert ! video/x-raw,format=BGR ! "
                "appsink name=vsink emit-signals=true sync=false max-buffers=8 drop=true "
                // 音声
                "dec. ! queue max-size-buffers=100 max-size-bytes=0 max-size-time=0 ! "
                "audioconvert ! audioresample ! audio/x-raw,format=S16LE,rate=" + std::to_string(SAMPLE_RATE) + ",channels=" + std::to_string(CHANNELS) + " ! "
                "appsink name=asink emit-signals=true sync=false max-buffers=100 drop=false";

            GError* err = nullptr;
            GstElement* pipeline2 = gst_parse_launch(pdesc.c_str(), &err);
            if (!pipeline2) {
                std::cerr << "[flv_srv] Failed to create pipeline: " << (err ? err->message : "unknown") << std::endl;
                if (err) g_error_free(err);
                return false;
            }
            if (err) { g_error_free(err); err = nullptr; }

            GstElement* vsink_el = gst_bin_get_by_name(GST_BIN(pipeline2), "vsink");
            GstElement* asink_el = gst_bin_get_by_name(GST_BIN(pipeline2), "asink");
            if (!vsink_el || !asink_el) {
                std::cerr << "[flv_srv] Failed to get appsink(s)" << std::endl;
                if (vsink_el) gst_object_unref(vsink_el);
                if (asink_el) gst_object_unref(asink_el);
                gst_object_unref(pipeline2);
                return false;
            }

            GstAppSink* vsink = GST_APP_SINK(vsink_el);
            GstAppSink* asink = GST_APP_SINK(asink_el);
            g_signal_connect(vsink, "new-sample", G_CALLBACK(on_new_sample_video), nullptr);
            g_signal_connect(asink, "new-sample", G_CALLBACK(on_new_sample_audio), nullptr);

            GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
            GstBus* bus = gst_element_get_bus(pipeline2);
            BusCtx bus_ctx; bus_ctx.loop = loop; bus_ctx.pipeline = pipeline2; bus_ctx.auto_restart = false; bus_ctx.request_restart = false;
            gst_bus_add_watch(bus, (GstBusFunc)on_bus_message, &bus_ctx);

            gst_element_set_state(pipeline2, GST_STATE_PLAYING);

            std::thread stopper([&]() {
                while (!g_should_exit) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                g_main_loop_quit(loop);
            });

            g_main_loop_run(loop);

            if (stopper.joinable()) stopper.join();
            gst_element_set_state(pipeline2, GST_STATE_NULL);
            gst_object_unref(bus);
            gst_object_unref(vsink_el);
            gst_object_unref(asink_el);
            gst_object_unref(pipeline2);
            g_main_loop_unref(loop);
            return true;
        };

        while (!g_should_exit) {
            sockaddr_in cli{}; socklen_t cl = sizeof(cli);
            int fd = ::accept(listen_fd, (sockaddr*)&cli, &cl);
            if (fd < 0) {
                if (errno == EINTR) continue;
                perror("accept");
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }
            std::cout << "[flv_srv] accepted from " << inet_ntoa(cli.sin_addr) << ":" << ntohs(cli.sin_port) << std::endl;
            int rcvbuf = 4 * 1024 * 1024; // 4MB socket receive buffer
            ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
            ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
            build_and_run_from_fd(fd);
            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
            std::cout << "[flv_srv] connection closed. Waiting next..." << std::endl;
        }

        ::close(listen_fd);
        // 通常の片付けへ続行
    } else if (mode == "raw") {
        // UDP rawvideo/gray8 受信例: ffmpeg -i ... -vf scale=24:4,format=gray -f rawvideo -pix_fmt gray - | nc -u ...
        pipeline_desc =
            "udpsrc port=" + std::to_string(port) + " ! "
            "queue2 ! "
            "videoparse width=" + std::to_string(active_config.total_width) +
            " height=" + std::to_string(active_config.total_height) +
            " format=gray8 framerate=30/1 ! "
            "appsink name=vsink emit-signals=true sync=false max-buffers=8 drop=true";
    } else if (mode == "stdin") {
        // 標準入力rawvideo/gray8受信例: ffmpeg ... | ./7seg-net-player stdin 24x4
        pipeline_desc =
            "fdsrc ! "
            "videoparse width=" + std::to_string(active_config.total_width) +
            " height=" + std::to_string(active_config.total_height) +
            " format=gray8 framerate=30/1 ! "
            "appsink name=vsink emit-signals=true sync=false max-buffers=8 drop=true";
    } else {
        std::cerr << "Unknown mode: " << mode << std::endl;
        return 1;
    }

    bool enable_restart = (mode == "flv" || mode == "flv_srv");
    int restart_delay_ms = []{
        int v = 800; if (const char* s = std::getenv("FLV_RESTART_MS")) v = std::max(100, atoi(s));
        return v;
    }();
    while (!g_should_exit) {
        GError* err = nullptr;
        GstElement* pipeline = gst_parse_launch(pipeline_desc.c_str(), &err);
        if (!pipeline) {
            std::cerr << "Failed to create pipeline: " << (err ? err->message : "unknown") << std::endl;
            if (err) g_error_free(err);
            if (enable_restart) {
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
                continue; // retry
            } else break;
        }
        if (err) { g_error_free(err); err = nullptr; }

        GstElement* vsink_el = gst_bin_get_by_name(GST_BIN(pipeline), "vsink");
        GstElement* asink_el = gst_bin_get_by_name(GST_BIN(pipeline), "asink");
        if (!vsink_el || !asink_el) {
            std::cerr << "Failed to get appsink(s)" << std::endl;
            if (vsink_el) gst_object_unref(vsink_el);
            if (asink_el) gst_object_unref(asink_el);
            gst_object_unref(pipeline);
            if (enable_restart) {
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
                continue; // retry
            } else break;
        }

        GstAppSink* vsink = GST_APP_SINK(vsink_el);
        GstAppSink* asink = GST_APP_SINK(asink_el);
        g_signal_connect(vsink, "new-sample", G_CALLBACK(on_new_sample_video), nullptr);
        g_signal_connect(asink, "new-sample", G_CALLBACK(on_new_sample_audio), nullptr);

        GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
        GstBus* bus = gst_element_get_bus(pipeline);
    BusCtx bus_ctx;
        bus_ctx.loop = loop;
        bus_ctx.pipeline = pipeline;
    bus_ctx.auto_restart = enable_restart;
        bus_ctx.request_restart = false;
        gst_bus_add_watch(bus, (GstBusFunc)on_bus_message, &bus_ctx);

        gst_element_set_state(pipeline, GST_STATE_PLAYING);

        std::thread stopper([&]() {
            while (!g_should_exit) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            g_main_loop_quit(loop);
        });

        g_main_loop_run(loop);

        if (stopper.joinable()) stopper.join();

        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(bus);
        gst_object_unref(vsink_el);
        gst_object_unref(asink_el);
        gst_object_unref(pipeline);
        g_main_loop_unref(loop);

        if (!(enable_restart && bus_ctx.request_restart) || g_should_exit) {
            break;
        }
        std::cout << "[GStreamer] restarting pipeline after EOS/ERROR... (delay=800ms)" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }

    if (vthr.joinable()) vthr.join();

    close(i2c_fd);
    audio_cleanup();

    std::cout << "\nプログラムを終了します。" << std::endl;
    return 0;
}
