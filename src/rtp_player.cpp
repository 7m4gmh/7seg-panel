#include "common.h"
#include "config.h"
#include "config_loader.hpp"
#include "led.h"
#include "video.h"
#include "audio.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <atomic>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>
#include <fcntl.h>

static std::atomic<bool> stop_flag(false);
static std::atomic<int> video_frame_count{0};

//------------- appsink callbacks -------------
static GstFlowReturn on_new_sample_video(GstAppSink* sink, gpointer) {
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (buffer) {
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            std::vector<uint8_t> jpg(map.data, map.data + map.size);

            int pts_ms = 0;
            if (GST_BUFFER_PTS_IS_VALID(buffer)) {
                pts_ms = static_cast<int>(GST_BUFFER_PTS(buffer) / GST_MSECOND);
            }

            {
                std::lock_guard<std::mutex> lk(frame_mtx);
                latest_frame = std::move(jpg);
                last_pts_ms = pts_ms;
            }

            int n = ++video_frame_count;
            if (n <= 3) {
                std::cout << "[video] received JPEG frame " << n
                          << " (" << map.size << " bytes, pts=" << pts_ms << " ms)" << std::endl;
            }

            gst_buffer_unmap(buffer, &map);
        }
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

static GstFlowReturn on_new_sample_audio(GstAppSink* sink, gpointer) {
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (buffer) {
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            if (map.size > 0) {
                audio_queue(reinterpret_cast<const char*>(map.data),
                            static_cast<int>(map.size));
            }
            gst_buffer_unmap(buffer, &map);
        }
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

//------------- bus handler -------------
static gboolean on_bus_message(GstBus*, GstMessage* msg, gpointer loop_ptr) {
    GMainLoop* loop = reinterpret_cast<GMainLoop*>(loop_ptr);
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr; gchar* dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            gchar* srcname = gst_object_get_path_string(GST_MESSAGE_SRC(msg));
            std::cerr << "[GStreamer][ERROR] from " << (srcname ? srcname : "?")
                      << ": " << (err ? err->message : "unknown") << std::endl;
            if (dbg) { std::cerr << "  debug: " << dbg << std::endl; g_free(dbg); }
            if (srcname) g_free(srcname);
            if (err) g_error_free(err);
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "[GStreamer] EOS" << std::endl;
            g_main_loop_quit(loop);
            break;
        default: break;
    }
    return TRUE;
}

//------------- pipeline builders (programmatic) -------------
static GstElement* build_video_pipeline(int port, GstAppSink** out_vsink) {
    GstElement* pipe = gst_pipeline_new("video-pipeline");
    if (!pipe) { std::cerr << "[video] failed to create pipeline" << std::endl; return nullptr; }

    GstElement* udpsrc = gst_element_factory_make("udpsrc", "vudpsrc");
    GstElement* caps1  = gst_element_factory_make("capsfilter", "vsrc_caps");
    GstElement* jitter = gst_element_factory_make("rtpjitterbuffer", "vjitter");
    GstElement* depay  = gst_element_factory_make("rtph264depay", "vdepay");
    GstElement* parse  = gst_element_factory_make("h264parse", "vparse");
    // Prefer HW decoder if available (Rockchip/NPU etc.), fallback to avdec_h264
    GstElement* dec    = gst_element_factory_make("mppvideodec", "vdec");
    if (!dec) dec = gst_element_factory_make("v4l2h264dec", "vdec");
    if (!dec) dec = gst_element_factory_make("avdec_h264", "vdec");
    GstElement* conv   = gst_element_factory_make("videoconvert", "vconv");
    GstElement* vscale = gst_element_factory_make("videoscale", "vscale");
    GstElement* vrate  = gst_element_factory_make("videorate", "vrate");
    GstElement* rawcf  = gst_element_factory_make("capsfilter", "vraw_caps");
    GstElement* jenc   = gst_element_factory_make("jpegenc", "vjpegenc");
    GstElement* jpgcf  = gst_element_factory_make("capsfilter", "vjpeg_caps");
    GstElement* queue  = gst_element_factory_make("queue", "vqueue");
    GstElement* sink   = gst_element_factory_make("appsink", "vsink");

    if (!udpsrc || !caps1 || !jitter || !depay || !parse || !dec || !conv || !vscale || !vrate || !rawcf || !jenc || !jpgcf || !queue || !sink) {
        std::cerr << "[video] missing GStreamer plugin(s). Check: udpsrc, rtpjitterbuffer, rtph264depay, h264parse, avdec_h264, videoconvert, videoscale, videorate, jpegenc, appsink" << std::endl;
        return nullptr;
    }

    // Properties
    g_object_set(udpsrc, "port", port, NULL);
    {
        GstCaps* c = gst_caps_from_string("application/x-rtp,media=video,encoding-name=H264,clock-rate=90000,payload=96");
        g_object_set(caps1, "caps", c, NULL);
        gst_caps_unref(c);
    }
    g_object_set(jitter, "latency", 200, "drop-on-late", TRUE, NULL);

    // h264parse defaults are fine; if needed: g_object_set(parse, "disable-passthrough", TRUE, NULL);

    // jpegenc properties (品質を少し下げて負荷軽減)
    g_object_set(jenc, "quality", 35, NULL);

    {
        // 画面を軽くするため、I420 320x240/15fps に正規化
        GstCaps* c = gst_caps_from_string("video/x-raw,format=I420,width=320,height=240,framerate=15/1");
        g_object_set(rawcf, "caps", c, NULL);
        gst_caps_unref(c);
    }
    {
        GstCaps* c = gst_caps_from_string("image/jpeg");
        g_object_set(jpgcf, "caps", c, NULL);
        gst_caps_unref(c);
    }

    // queue properties: 少し余裕を持たせる
    g_object_set(queue, "leaky", 2, "max-size-buffers", 8, NULL);

    // appsink: 少し多めのバッファでドロップ許容（レイテンシを抑えつつカクつき緩和）
    g_object_set(sink, "emit-signals", TRUE, "sync", FALSE, "max-buffers", 8, "drop", TRUE, NULL);

    gst_bin_add_many(GST_BIN(pipe),
                     udpsrc, caps1, jitter, depay, parse, dec, conv, vscale, vrate, rawcf, jenc, jpgcf, queue, sink, NULL);

    auto link_or_log = [](GstElement* a, GstElement* b, const char* aname, const char* bname) -> bool {
        if (!gst_element_link(a, b)) {
            std::cerr << "[video] failed to link " << aname << " -> " << bname << std::endl;
            return false;
        }
        return true;
    };

    if (!link_or_log(udpsrc, caps1,  "vudpsrc", "vsrc_caps")) return nullptr;
    if (!link_or_log(caps1,  jitter, "vsrc_caps", "vjitter")) return nullptr;
    if (!link_or_log(jitter, depay,  "vjitter", "vdepay")) return nullptr;
    if (!link_or_log(depay,  parse,  "vdepay", "vparse")) return nullptr;
    if (!link_or_log(parse,  dec,    "vparse", "vdec")) return nullptr;
    if (!link_or_log(dec,    conv,   "vdec", "vconv")) return nullptr;
    if (!link_or_log(conv,   vscale, "vconv", "vscale")) return nullptr;
    if (!link_or_log(vscale, vrate,  "vscale", "vrate")) return nullptr;
    if (!link_or_log(vrate,  rawcf,  "vrate", "vraw_caps")) return nullptr;
    if (!link_or_log(rawcf,  jenc,   "vraw_caps", "vjpegenc")) return nullptr;
    if (!link_or_log(jenc,   jpgcf,  "vjpegenc", "vjpeg_caps")) return nullptr;
    if (!link_or_log(jpgcf,  queue,  "vjpeg_caps", "vqueue")) return nullptr;
    if (!link_or_log(queue,  sink,   "vqueue", "vsink")) return nullptr;

    if (out_vsink) *out_vsink = GST_APP_SINK(sink);
    return pipe;
}

static GstElement* build_audio_pipeline(int port, GstAppSink** out_asink) {
    GstElement* pipe = gst_pipeline_new("audio-pipeline");
    if (!pipe) { std::cerr << "[audio] failed to create pipeline" << std::endl; return nullptr; }

    GstElement* udpsrc = gst_element_factory_make("udpsrc", "audpsrc");
    GstElement* caps1  = gst_element_factory_make("capsfilter", "asrc_caps");
    GstElement* jitter = gst_element_factory_make("rtpjitterbuffer", "ajitter");
    GstElement* depay  = gst_element_factory_make("rtpopusdepay", "adepay");
    GstElement* dec    = gst_element_factory_make("opusdec", "adec");
    GstElement* aconv  = gst_element_factory_make("audioconvert", "aconv");
    GstElement* ares   = gst_element_factory_make("audioresample", "ares");
    GstElement* rawcf  = gst_element_factory_make("capsfilter", "araw_caps");
    GstElement* sink   = gst_element_factory_make("appsink", "asink");

    if (!udpsrc || !caps1 || !jitter || !depay || !dec || !aconv || !ares || !rawcf || !sink) {
        std::cerr << "[audio] missing GStreamer plugin(s). Check: udpsrc, rtpjitterbuffer, rtpopusdepay, opusdec, audioconvert, audioresample, appsink" << std::endl;
        return nullptr;
    }

    g_object_set(udpsrc, "port", port, NULL);
    {
        GstCaps* c = gst_caps_from_string("application/x-rtp,media=audio,encoding-name=OPUS,clock-rate=48000,payload=97");
        g_object_set(caps1, "caps", c, NULL);
        gst_caps_unref(c);
    }
    g_object_set(jitter, "latency", 120, NULL);
    // Opus decoder options: Packet Loss Concealment (PLC) and Forward Error Correction (FEC)
    g_object_set(dec, "use-inband-fec", TRUE, "plc", TRUE, NULL);

    {
        GstCaps* c = gst_caps_from_string("audio/x-raw,format=S16LE,rate=48000,channels=2");
        g_object_set(rawcf, "caps", c, NULL);
        gst_caps_unref(c);
    }

    g_object_set(sink, "emit-signals", TRUE, "sync", FALSE, "max-buffers", 64, "drop", FALSE, NULL);

    gst_bin_add_many(GST_BIN(pipe),
                     udpsrc, caps1, jitter, depay, dec, aconv, ares, rawcf, sink, NULL);

    auto link_or_log = [](GstElement* a, GstElement* b, const char* aname, const char* bname) -> bool {
        if (!gst_element_link(a, b)) {
            std::cerr << "[audio] failed to link " << aname << " -> " << bname << std::endl;
            return false;
        }
        return true;
    };

    if (!link_or_log(udpsrc, caps1,  "audpsrc", "asrc_caps")) return nullptr;
    if (!link_or_log(caps1,  jitter, "asrc_caps", "ajitter")) return nullptr;
    if (!link_or_log(jitter, depay,  "ajitter", "adepay")) return nullptr;
    if (!link_or_log(depay,  dec,    "adepay", "adec")) return nullptr;
    if (!link_or_log(dec,    aconv,  "adec", "aconv")) return nullptr;
    if (!link_or_log(aconv,  ares,   "aconv", "ares")) return nullptr;
    if (!link_or_log(ares,   rawcf,  "ares", "araw_caps")) return nullptr;
    if (!link_or_log(rawcf,  sink,   "araw_caps", "asink")) return nullptr;

    if (out_asink) *out_asink = GST_APP_SINK(sink);
    return pipe;
}

//------------- main -------------
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
    if (i2c_fd < 0) { perror("Failed to open /dev/i2c-0"); return 1; }
    if (!initialize_displays(i2c_fd, active_config)) {
        std::cerr << "Failed to initialize display modules." << std::endl;
        close(i2c_fd); return 1;
    }

    setup_signal_handlers();

    if (!audio_init(SAMPLE_RATE, CHANNELS)) {
        std::cerr << "Audio init failed" << std::endl;
    }

    std::thread vthr(video_thread, std::ref(i2c_fd), std::ref(active_config), std::ref(stop_flag));

    gst_init(&argc, &argv);

    GstAppSink* vsink = nullptr;
    GstAppSink* asink = nullptr;

    GstElement* vpipe = build_video_pipeline(video_port, &vsink);
    GstElement* apipe = build_audio_pipeline(audio_port, &asink);
    if (!vpipe || !apipe || !vsink || !asink) {
        std::cerr << "Failed to build pipelines" << std::endl;
        if (vpipe) gst_object_unref(vpipe);
        if (apipe) gst_object_unref(apipe);
        stop_flag = true;
        if (vthr.joinable()) vthr.join();
        close(i2c_fd); audio_cleanup();
        return 1;
    }

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
        while (!g_should_exit) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        g_main_loop_quit(loop);
    });

    g_main_loop_run(loop);

    stop_flag = true;

    if (stopper.joinable()) stopper.join();

    gst_element_set_state(vpipe, GST_STATE_NULL);
    gst_element_set_state(apipe, GST_STATE_NULL);

    gst_object_unref(vbus);
    gst_object_unref(abus);
    gst_object_unref(vpipe);
    gst_object_unref(apipe);
    g_main_loop_unref(loop);

    if (vthr.joinable()) vthr.join();

    close(i2c_fd);
    audio_cleanup();

    std::cout << "\nプログラムを終了します。" << std::endl;
    return 0;
}