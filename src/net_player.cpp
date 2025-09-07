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

// common.h で宣言されているグローバル変数と関数を使用します
// static std::atomic<bool> stop_flag(false);
// static std::atomic<bool> g_should_exit(false);
// static void setup_signal_handlers();

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

static gboolean on_bus_message(GstBus* bus, GstMessage* msg, gpointer loop_ptr) {
    GMainLoop* loop = reinterpret_cast<GMainLoop*>(loop_ptr);
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
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "[GStreamer] EOS" << std::endl;
            g_main_loop_quit(loop);
            break;
        default:
            break;
    }
    return TRUE;
}

int main(int argc, char* argv[]) {
    // 使い方:
    // ./7seg-net-player [config_name] [port]
    // 例: ./7seg-net-player 16x16_expanded 5004
    std::string config_name = (argc > 1) ? argv[1] : "24x4";
    int port = (argc > 2) ? std::stoi(argv[2]) : 5004;

    DisplayConfig active_config;
    try {
        active_config = load_config_from_json(config_name);
        std::cout << "Successfully loaded configuration: " << active_config.name << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return 1;
    }

    int i2c_fd = open_i2c_auto();
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

    std::string pipeline_desc =
        // UDP TS は RTP と異なりジッタバッファが無い => do-timestamp + queue2 + tsparse で到着時刻付与とバッファリング
        "udpsrc port=" + std::to_string(port) + " caps=\"video/mpegts,systemstream=true,packetsize=188\" buffer-size=2097152 do-timestamp=true ! "
        "queue2 use-buffering=true max-size-time=1500000000 max-size-buffers=0 max-size-bytes=0 ! "
        "tsparse set-timestamps=true ! tsdemux name=demux "
        // 映像ブランチ
        "demux. ! queue2 use-buffering=true max-size-time=1000000000 max-size-buffers=0 max-size-bytes=0 ! "
        "h264parse config-interval=-1 disable-passthrough=true ! avdec_h264 ! videoconvert ! video/x-raw,format=BGR ! "
        "appsink name=vsink emit-signals=true sync=false max-buffers=8 drop=true "
        // 音声ブランチ（音飛び対策でバッファ多め、dropしない）
        "demux. ! queue2 use-buffering=true max-size-time=1500000000 max-size-buffers=0 max-size-bytes=0 ! "
        "aacparse ! avdec_aac ! audioconvert ! audioresample ! audio/x-raw,format=S16LE,rate=" + std::to_string(SAMPLE_RATE) + ",channels=" + std::to_string(CHANNELS) + " ! "
        "appsink name=asink emit-signals=true sync=false max-buffers=100 drop=false";

    GError* err = nullptr;
    GstElement* pipeline = gst_parse_launch(pipeline_desc.c_str(), &err);
    if (!pipeline) {
        std::cerr << "Failed to create pipeline: " << (err ? err->message : "unknown") << std::endl;
        if (err) g_error_free(err);
        g_should_exit = true;
        if (vthr.joinable()) vthr.join();
        close(i2c_fd);
        audio_cleanup();
        return 1;
    }
    if (err) { g_error_free(err); err = nullptr; }

    GstElement* vsink_el = gst_bin_get_by_name(GST_BIN(pipeline), "vsink");
    GstElement* asink_el = gst_bin_get_by_name(GST_BIN(pipeline), "asink");
    if (!vsink_el || !asink_el) {
        std::cerr << "Failed to get appsink(s)" << std::endl;
        if (vsink_el) gst_object_unref(vsink_el);
        if (asink_el) gst_object_unref(asink_el);
        gst_object_unref(pipeline);
        g_should_exit = true;
        if (vthr.joinable()) vthr.join();
        close(i2c_fd);
        audio_cleanup();
        return 1;
    }

    GstAppSink* vsink = GST_APP_SINK(vsink_el);
    GstAppSink* asink = GST_APP_SINK(asink_el);
    g_signal_connect(vsink, "new-sample", G_CALLBACK(on_new_sample_video), nullptr);
    g_signal_connect(asink, "new-sample", G_CALLBACK(on_new_sample_audio), nullptr);

    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_add_watch(bus, (GstBusFunc)on_bus_message, loop);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    std::thread stopper([&]() {
        while (!g_should_exit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        g_main_loop_quit(loop);
    });

    g_main_loop_run(loop);

    // stop_flag = true; // g_should_exit を使う

    if (stopper.joinable()) stopper.join();

    gst_element_set_state(pipeline, GST_STATE_NULL);

    gst_object_unref(bus);
    gst_object_unref(vsink_el);
    gst_object_unref(asink_el);
    gst_object_unref(pipeline);
    g_main_loop_unref(loop);

    if (vthr.joinable()) vthr.join();

    close(i2c_fd);
    audio_cleanup();

    std::cout << "\nプログラムを終了します。" << std::endl;
    return 0;
}
