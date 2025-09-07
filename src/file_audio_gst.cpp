#include "file_audio_gst.h"
#include "audio.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <atomic>
#include <thread>
#include <iostream>

static std::atomic<bool> g_run{false};
static std::thread g_loop_thread;
static GstElement* g_pipeline = nullptr;

static GstFlowReturn on_new_sample(GstAppSink* sink, gpointer) {
    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) return GST_FLOW_OK;
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (buffer) {
        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            if (map.size > 0) {
                audio_queue(reinterpret_cast<const char*>(map.data), (int)map.size);
            }
            gst_buffer_unmap(buffer, &map);
        }
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

bool file_audio_start(const std::string& filepath) {
    if (g_run.load()) return true;
    gst_init(nullptr, nullptr);

    std::string desc =
        "filesrc location=\"" + filepath + "\" ! decodebin name=d "
        "d. ! queue ! audioconvert ! audioresample ! audio/x-raw,format=S16LE,rate=48000,channels=2 ! "
        "appsink name=asink emit-signals=true sync=false max-buffers=64 drop=false";

    GError* err = nullptr;
    g_pipeline = gst_parse_launch(desc.c_str(), &err);
    if (!g_pipeline) {
        if (err) {
            std::cerr << "[file-audio] pipeline error: " << err->message << std::endl;
            g_error_free(err);
        }
        return false;
    }
    if (err) g_error_free(err);

    GstElement* asink_el = gst_bin_get_by_name(GST_BIN(g_pipeline), "asink");
    if (!asink_el) {
        std::cerr << "[file-audio] appsink not found" << std::endl;
        gst_object_unref(g_pipeline); g_pipeline = nullptr; return false;
    }
    GstAppSink* asink = GST_APP_SINK(asink_el);
    g_signal_connect(asink, "new-sample", G_CALLBACK(on_new_sample), nullptr);

    g_run = true;
    g_loop_thread = std::thread([]{
        GstBus* bus = gst_element_get_bus(g_pipeline);
        gst_element_set_state(g_pipeline, GST_STATE_PLAYING);
        while (g_run) {
            GstMessage* msg = gst_bus_timed_pop_filtered(bus, 100 * GST_MSECOND,
                (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
            if (!msg) continue;
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError* e; gchar* dbg;
                    gst_message_parse_error(msg, &e, &dbg);
                    std::cerr << "[file-audio] ERROR: " << (e?e->message:"?") << std::endl;
                    if (dbg) {
                        g_free(dbg);
                    }
                    if (e) {
                        g_error_free(e);
                    }
                    g_run = false;
                    break;
                }
                case GST_MESSAGE_EOS:
                    g_run = false; break;
                default: break;
            }
            gst_message_unref(msg);
        }
        gst_element_set_state(g_pipeline, GST_STATE_NULL);
        gst_object_unref(bus);
    });

    return true;
}

void file_audio_stop() {
    if (!g_run && !g_pipeline) return;
    g_run = false;
    if (g_loop_thread.joinable()) g_loop_thread.join();
    if (g_pipeline) { gst_object_unref(g_pipeline); g_pipeline = nullptr; }
}
