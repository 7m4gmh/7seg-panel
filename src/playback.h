// src/playback.h
#ifndef PLAYBACK_H
#define PLAYBACK_H
#include <string>
#include <atomic>
#include "common.h"

// 既存のファイル再生エンジン
int play_video_stream(const std::string& video_path, const DisplayConfig& config, std::atomic<bool>& stop_flag);


#endif // PLAYBACK_H