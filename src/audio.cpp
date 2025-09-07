#include "audio.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <thread>

static SDL_AudioDeviceID dev = 0;
static SDL_AudioSpec obtained;
static int g_frame_bytes = 0;              // 1フレーム(全ch)のバイト数
static int g_fadein_frames_remaining = 0;  // フェードイン残りフレーム数
static int g_bytes_per_sec = 0;
// DCブロッカー用（一次ハイパス）
static float hp_R = 0.995f; // 時定数係数（0.99〜0.999の範囲で調整）
static float hp_xL_prev = 0.0f, hp_yL_prev = 0.0f;
static float hp_xR_prev = 0.0f, hp_yR_prev = 0.0f;

static inline void dc_blocker_s16(int16_t* samples, size_t frames, int channels) {
    for (size_t f = 0; f < frames; ++f) {
        // L
        float xL = samples[f * channels + 0];
        float yL = xL - hp_xL_prev + hp_R * hp_yL_prev;
        hp_xL_prev = xL; hp_yL_prev = yL;
        int sL = (int)lroundf(yL);
        if (sL > 32767) sL = 32767; else if (sL < -32768) sL = -32768;
        samples[f * channels + 0] = (int16_t)sL;
        if (channels > 1) {
            float xR = samples[f * channels + 1];
            float yR = xR - hp_xR_prev + hp_R * hp_yR_prev;
            hp_xR_prev = xR; hp_yR_prev = yR;
            int sR = (int)lroundf(yR);
            if (sR > 32767) sR = 32767; else if (sR < -32768) sR = -32768;
            samples[f * channels + 1] = (int16_t)sR;
        }
    }
}

bool audio_init(int samplerate, int channels) {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq = samplerate;
    desired.format = AUDIO_S16SYS;   // 16-bit PCM
    desired.channels = channels;
    desired.samples = 2048; // buffer size (増やしてジッタに強く)
    desired.callback = nullptr; // push型

    dev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (dev == 0) {
        std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // 1) 初期サイレンスをプリキュー（環境変数で調整: AUDIO_PRIME_MS）
    const int bytes_per_sample = SDL_AUDIO_BITSIZE(obtained.format) / 8; // =2 (S16)
    g_frame_bytes = bytes_per_sample * obtained.channels;                // 1フレーム=全ch分
    int prime_ms = 100;
    if (const char* v = std::getenv("AUDIO_PRIME_MS")) { prime_ms = std::max(0, atoi(v)); }
    const int prime_frames = (obtained.freq * prime_ms) / 1000;
    std::vector<uint8_t> silence(prime_frames * g_frame_bytes, 0);
    if (SDL_QueueAudio(dev, silence.data(), silence.size()) < 0) {
        std::cerr << "SDL_QueueAudio(silence) error: " << SDL_GetError() << std::endl;
    }

    // 2) フェードイン（環境変数で調整: AUDIO_FADEIN_MS）
    int fade_ms = 30;
    if (const char* v = std::getenv("AUDIO_FADEIN_MS")) { fade_ms = std::max(0, atoi(v)); }
    g_fadein_frames_remaining = (obtained.freq * fade_ms) / 1000;

    g_bytes_per_sec = obtained.freq * g_frame_bytes;

    // サイレンスが入っている状態で再生開始（クリック低減）
    SDL_PauseAudioDevice(dev, 0);
    std::cout << "Audio device opened: "
              << obtained.freq << " Hz, "
              << (int)obtained.channels << " ch"
              << std::endl;
    return true;
}

void audio_queue(const char* data, size_t len) {
    if (dev) {
        if (g_fadein_frames_remaining > 0 && g_frame_bytes > 0 && len >= (size_t)g_frame_bytes) {
            // 3) 最初の数十msだけフェードインを適用
            const size_t frames = len / g_frame_bytes;
            std::vector<int16_t> tmp(len / sizeof(int16_t));
            // 元データを16bitに解釈してコピー
            std::memcpy(tmp.data(), data, tmp.size() * sizeof(int16_t));

            int16_t* samples = tmp.data();
            const int channels = obtained.channels;
            const int ramp_total = std::max(1, g_fadein_frames_remaining + (int)frames); // 進行に応じて滑らかに
            int ramp_left = g_fadein_frames_remaining;
            for (size_t f = 0; f < frames && ramp_left > 0; ++f) {
                double gain = 1.0 - (double)ramp_left / (double)ramp_total; // 0→1 へ線形
                if (gain < 0.0) gain = 0.0;
                if (gain > 1.0) gain = 1.0;
                for (int c = 0; c < channels; ++c) {
                    size_t idx = f * channels + c;
                    int s = (int)samples[idx];
                    s = (int)(s * gain);
                    if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
                    samples[idx] = (int16_t)s;
                }
                --ramp_left;
            }
            g_fadein_frames_remaining = std::max(0, g_fadein_frames_remaining - (int)frames);

            // DCブロッカーを適用
            dc_blocker_s16(samples, frames, channels);

            if (SDL_QueueAudio(dev, (const void*)samples, len) < 0) {
                std::cerr << "SDL_QueueAudio(fadein) error: " << SDL_GetError() << std::endl;
            }
        } else {
            // 非フェード時もDCブロッカー
            if (g_frame_bytes > 0) {
                const size_t frames = len / g_frame_bytes;
                std::vector<int16_t> tmp(len / sizeof(int16_t));
                std::memcpy(tmp.data(), data, tmp.size() * sizeof(int16_t));
                dc_blocker_s16(tmp.data(), frames, obtained.channels);
                if (SDL_QueueAudio(dev, (const void*)tmp.data(), len) < 0) {
                    std::cerr << "SDL_QueueAudio error: " << SDL_GetError() << std::endl;
                }
            } else if (SDL_QueueAudio(dev, data, len) < 0) {
                std::cerr << "SDL_QueueAudio error: " << SDL_GetError() << std::endl;
            }
        }
    }
}

void audio_cleanup() {
    if (dev) {
        // 残りキューを滑らかにドレインしてから停止
        const Uint32 queued = SDL_GetQueuedAudioSize(dev);
        if (queued > 0 && g_bytes_per_sec > 0) {
            // 追加で短い無音を足して穏やかにゼロへ（環境変数: AUDIO_TAIL_MS）
            int tail_ms = 40;
            if (const char* v = std::getenv("AUDIO_TAIL_MS")) { tail_ms = std::max(0, atoi(v)); }
            const int tail_frames = (obtained.freq * tail_ms) / 1000;
            std::vector<uint8_t> tail_silence(tail_frames * g_frame_bytes, 0);
            SDL_QueueAudio(dev, tail_silence.data(), tail_silence.size());

            // 最大でも200ms程度待つ
            double wait_s = (double)(queued + tail_silence.size()) / (double)g_bytes_per_sec;
            if (const char* v = std::getenv("AUDIO_ANTI_POP_MAX_WAIT_MS")) {
                int w = std::max(0, atoi(v));
                wait_s = std::min(wait_s, w / 1000.0);
            } else {
                wait_s = std::min(wait_s, 0.2);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(wait_s * 1000)));
        }
        // 再生を一時停止し、キューをクリアしてからクローズ
        SDL_PauseAudioDevice(dev, 1);
        SDL_ClearQueuedAudio(dev);
        SDL_CloseAudioDevice(dev);
        dev = 0;
    }
    SDL_Quit();
}

