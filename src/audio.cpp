#include "common.h"
#include <portaudio.h>
#include <vector>
#include <cstring>
#include <thread>
#include <mutex>
#include <deque>

// --- Audioコールバック ---
static int paCallback(const void *input,
                      void *output,
                      unsigned long frameCount,
                      const PaStreamCallbackTimeInfo* timeInfo,
                      PaStreamCallbackFlags statusFlags,
                      void *userData )
{
    unsigned char* out = static_cast<unsigned char*>(output);
    size_t need = frameCount * CHANNELS * sizeof(int16_t);

    std::vector<char> chunk;
    {
        std::lock_guard<std::mutex> lock(audio_mtx);
        if(!audio_buf.empty()){
            chunk = audio_buf.front();
            audio_buf.pop_front();
        } else {
            chunk.resize(need,0); // 無音
        }
    }
    if(chunk.size()<need) chunk.resize(need,0);
    memcpy(out, chunk.data(), need);

    return finished? paComplete : paContinue;
}

void start_audio() {
    Pa_Initialize();
    PaStream* stream;
    PaStreamParameters outputParameters;
    outputParameters.device = Pa_GetDefaultOutputDevice();
    outputParameters.channelCount = CHANNELS;
    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency =
        Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    Pa_OpenStream(&stream, NULL, &outputParameters,
                  SAMPLE_RATE,
                  AUDIO_CHUNK_SIZE/(CHANNELS*2),
                  paClipOff, paCallback, NULL);

    Pa_StartStream(stream);
    while(!finished) {
        Pa_Sleep(100);
    }
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
