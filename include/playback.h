// src/playback.h
#pragma once

#ifndef PLAYBACK_H
#define PLAYBACK_H
#include <string>
#include <atomic>
#include "common.h"

extern std::atomic<bool>* g_current_stop_flag;

// スケーリングモード
enum class ScalingMode {
    CROP,    // アスペクト比を維持してトリミング
    STRETCH, // アスペクト比を無視してリサイズ
    FIT      // アスペクト比を維持して全体を表示（余白可能）
};

// 既存のファイル再生エンジン
int play_video_stream(const std::string& video_path, const DisplayConfig& config, std::atomic<bool>& stop_flag, 
                     ScalingMode scaling_mode = ScalingMode::CROP, int min_threshold = 64, int max_threshold = 255, bool debug = false);

// エミュレータ用の再生エンジン
int play_video_stream_emulator(const std::string& video_path, const DisplayConfig& config, std::atomic<bool>& stop_flag,
                              ScalingMode scaling_mode = ScalingMode::CROP, int min_threshold = 64, int max_threshold = 255, bool debug = false);


#endif // PLAYBACK_H