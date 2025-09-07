#include "audio.h"
#include <SDL2/SDL.h>
#include <iostream>

static SDL_AudioDeviceID dev = 0;
static SDL_AudioSpec obtained;

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

    dev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (dev == 0) {
        std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_PauseAudioDevice(dev, 0);  // 再生開始
    std::cout << "Audio device opened: "
              << obtained.freq << " Hz, "
              << (int)obtained.channels << " ch"
              << std::endl;
    return true;
}

void audio_queue(const char* data, size_t len) {
    if (dev) {
        if (SDL_QueueAudio(dev, data, len) < 0) {
            std::cerr << "SDL_QueueAudio error: " << SDL_GetError() << std::endl;
        }
    }
}

void audio_cleanup() {
    if (dev) {
        SDL_CloseAudioDevice(dev);
        dev = 0;
    }
    SDL_Quit();
}

