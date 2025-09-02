// src/playback.h
#pragma once

#ifndef PLAYBACK_H
#define PLAYBACK_H
#include <string>
#include <atomic>
#include "common.h"

extern std::atomic<bool>* g_current_stop_flag;

// 既存のファイル再生エンジン
int play_video_stream(const std::string& video_path, const DisplayConfig& config, std::atomic<bool>& stop_flag);


#endif // PLAYBACK_H