#pragma once

#include <vector>
#include <SDL3/SDL_audio.h>
#include "common.hpp"
#include "sdl_error.hpp"
#include "log.hpp"

namespace rana {
namespace audio {

static SDL_AudioStream *stream;

void init(int sample_rate)
{
    SDL_Init(SDL_INIT_AUDIO);

    const SDL_AudioSpec spec = { SDL_AUDIO_F32, 2, sample_rate };
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, 0, 0);
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));
}

void queue(std::vector<SampleStereo> samples)
{
    SDL_PutAudioStreamData(stream, samples.data(), samples.size() * sizeof(SampleStereo));
}

bool needs_more_data()
{
    int queue = SDL_GetAudioStreamQueued(stream);
    if (queue < sizeof(float) * 2 * 1024)
        return true;
    return false;
}

}
}
