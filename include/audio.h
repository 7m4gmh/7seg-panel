#pragma once
#include <cstddef>

bool audio_init(int samplerate=44100, int channels=2);
void audio_queue(const char* data, size_t len);
void audio_cleanup();

