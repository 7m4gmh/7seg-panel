// src/playback.h
#pragma once

#include "config.h"
#include <string>
#include <atomic>

/**
 * @brief 指定された動画ファイルを7セグパネルにストリーミング再生する
 * 
 * @param video_path 再生する動画ファイルのパス
 * @param config 使用するディスプレイ構成
 * @param stop_flag このatomic boolがtrueになると、再生を中断して関数を抜ける
 * @return int 成功時は0, エラー時は-1
 */
int play_video_stream(const std::string& video_path, const DisplayConfig& config, std::atomic<bool>& stop_flag);


