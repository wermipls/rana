#pragma once

#include <vector>
#include <SDL3/SDL_audio.h>
#include "common.hpp"
#include "sdl_error.hpp"
#include "log.hpp"

namespace rana {
namespace audio {

static SDL_AudioStream *stream;

void init(int sample_rate, SDL_AudioStreamCallback callback, void *userdata)
{
    SDL_Init(SDL_INIT_AUDIO);

    const SDL_AudioSpec spec = { SDL_AUDIO_F32, 2, sample_rate };
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, callback, userdata);
    if (!stream) {
        sdl_error("failed to open audio device stream");
        return;
    }
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
}

void deinit()
{
    SDL_DestroyAudioStream(stream);
    stream = nullptr;
}

}
}
