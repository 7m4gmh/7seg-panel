#include "file_audio_gst.h"

// GStreamerを使わないターゲット向けの弱いスタブ
// 実体（file_audio_gst.cpp）がリンクされる場合はそちらが優先されます。

__attribute__((weak)) bool file_audio_start(const std::string&) {
    return false;
}

__attribute__((weak)) void file_audio_stop() {
}
