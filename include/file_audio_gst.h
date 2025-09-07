#pragma once

#include <string>

// ファイルの音声を GStreamer でデコードし、SDL(QueueAudio)へ送る簡易モジュール
// 成功: true / 失敗: false
bool file_audio_start(const std::string& filepath);
void file_audio_stop();
