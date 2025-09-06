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

static std::atomic<bool> stop_flag(false);

static GstFlowReturn on_new_sample_video(GstAppSink* sink, gpointer) {
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    if (!buffer || !caps) {
        if (sample) gst_sample_unref(sample);
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

    int width  = GST_VIDEO_INFO_WIDTH(&vinfo);
    int height = GST_VIDEO_INFO_HEIGHT(&vinfo);

    // appsinkはBGRで受ける設定
    cv::Mat img(height, width, CV_8UC3, const_cast<guint8*>(map.data));
    std::vector<uint8_t> jpg;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 45};
    cv::imencode(".jpg", img, jpg, params);

    int pts_ms = 0;
    if (GST_BUFFER_PTS_IS_VALID(buffer)) {
        pts_ms = static_cast<int>(GST_BUFFER_PTS(buffer) / GST_MSECOND);
    }

    {
        std::lock_guard<std::mutex> lk(frame_mtx);
        if (!jpg.empty()) latest_frame = std::move(jpg);
        last_pts_ms = pts_ms;
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

static GstFlowReturn on_new_sample_audio(GstAppSink* sink, gpointer) {
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        if (map.size > 0) {
            audio_queue(reinterpret_cast<const char*>(map.data),
                        static_cast<int>(map.size));
        }
        gst_buffer_unmap(buffer, &map);
    }

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

static std::string make_video_pipeline_desc(int port) {
    return
        "udpsrc port=" + std::to_string(port) + " "
        "caps=application/x-rtp,media=video,encoding-name=H264,clock-rate=90000,payload=96 "
        "! rtpjitterbuffer latency=80 "
        "! rtph264depay ! h264parse disable-passthrough=true "
        "! avdec_h264 "
        "! videoconvert ! video/x-raw,format=BGR "
        "! appsink name=vsink emit-signals=true sync=false max-buffers=4 drop=true";
}

static std::string make_audio_pipeline_desc(int port) {
    return
        "udpsrc port=" + std::to_string(port) + " "
        "caps=application/x-rtp,media=audio,encoding-name=OPUS,clock-rate=48000,payload=97 "
        "! rtpjitterbuffer latency=80 "
        "! rtpopusdepay ! opusdec "
        "! audioconvert ! audioresample "
        "! audio/x-raw,format=S16LE,rate=" + std::to_string(SAMPLE_RATE) +
        ",channels=" + std::to_string(CHANNELS) + " "
        "! appsink name=asink emit-signals=true sync=false max-buffers=16 drop=true";
}

int main(int argc, char* argv[]) {
    // ./7seg-rtp-player [config_name] [video_port] [audio_port]
    std::string config_name = (argc > 1) ? argv[1] : "24x4";
    int video_port = (argc > 2) ? std::stoi(argv[2]) : 5004;
    int audio_port = (argc > 3) ? std::stoi(argv[3]) : 5006;

    DisplayConfig active_config;
    try {
        active_config = load_config_from_json(config_name);
        std::cout << "Successfully loaded configuration: " << active_config.name << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error loading configuration: " << e.what() << std::endl;
        return 1;
    }

    int i2c_fd = open("/dev/i2c-0", O_RDWR);
    if (i2c_fd < 0) {
        perror("Failed to open /dev/i2c-0");
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

    std::thread vthr(video_thread, std::ref(i2c_fd), std::ref(active_config), std::ref(stop_flag));

    gst_init(&argc, &argv);

    std::string vpipe_desc = make_video_pipeline_desc(video_port);
    std::string apipe_desc = make_audio_pipeline_desc(audio_port);

    GError* err = nullptr;
    GstElement* vpipe = gst_parse_launch(vpipe_desc.c_str(), &err);
    if (!vpipe) {
        std::cerr << "Failed to create video pipeline: " << (err ? err->message : "unknown") << std::endl;
        if (err) g_error_free(err);
        stop_flag = true;
        if (vthr.joinable()) vthr.join();
        close(i2c_fd);
        audio_cleanup();
        return 1;
    }
    if (err) { g_error_free(err); err = nullptr; }

    GstElement* apipe = gst_parse_launch(apipe_desc.c_str(), &err);
    if (!apipe) {
        std::cerr << "Failed to create audio pipeline: " << (err ? err->message : "unknown") << std::endl;
        if (err) g_error_free(err);
        gst_object_unref(vpipe);
        stop_flag = true;
        if (vthr.joinable()) vthr.join();
        close(i2c_fd);
        audio_cleanup();
        return 1;
    }
    if (err) { g_error_free(err); err = nullptr; }

    GstElement* vsink_el = gst_bin_get_by_name(GST_BIN(vpipe), "vsink");
    GstElement* asink_el = gst_bin_get_by_name(GST_BIN(apipe), "asink");
    if (!vsink_el || !asink_el) {
        std::cerr << "Failed to get appsink(s)" << std::endl;
        if (vsink_el) gst_object_unref(vsink_el);
        if (asink_el) gst_object_unref(asink_el);
        gst_object_unref(vpipe);
        gst_object_unref(apipe);
        stop_flag = true;
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
    GstBus* vbus = gst_element_get_bus(vpipe);
    GstBus* abus = gst_element_get_bus(apipe);
    gst_bus_add_watch(vbus, (GstBusFunc)on_bus_message, loop);
    gst_bus_add_watch(abus, (GstBusFunc)on_bus_message, loop);

    gst_element_set_state(vpipe, GST_STATE_PLAYING);
    gst_element_set_state(apipe, GST_STATE_PLAYING);

    std::thread stopper([&]() {
        while (!g_should_exit) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        g_main_loop_quit(loop);
    });

    g_main_loop_run(loop);

    stop_flag = true;

    if (stopper.joinable()) stopper.join();

    gst_element_set_state(vpipe, GST_STATE_NULL);
    gst_element_set_state(apipe, GST_STATE_NULL);

    gst_object_unref(vbus);
    gst_object_unref(abus);
    gst_object_unref(vsink_el);
    gst_object_unref(asink_el);
    gst_object_unref(vpipe);
    gst_object_unref(apipe);
    g_main_loop_unref(loop);

    if (vthr.joinable()) vthr.join();

    close(i2c_fd);
    audio_cleanup();

    std::cout << "\nプログラムを終了します。" << std::endl;
    return 0;
}