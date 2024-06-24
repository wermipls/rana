#pragma once

#include <vector>
#include <cmath>
#include <cstring>
#include <SDL2/SDL.h>
#include "log.hpp"
#include "common.hpp"

namespace rana {
namespace audio {

static SDL_AudioDeviceID device;
static SDL_AudioSpec device_spec;

void init(int sample_rate)
{
    SDL_Init(SDL_INIT_AUDIO);

    SDL_AudioSpec desired{};
    desired.channels = 2;
    desired.format = AUDIO_F32SYS;
    desired.freq = sample_rate;
    desired.samples = 2048;
    desired.callback = NULL;

    device = SDL_OpenAudioDevice(NULL, 0, &desired, &device_spec, 0);
    SDL_PauseAudioDevice(device, 0);
}

void queue(std::vector<SampleStereo> samples)
{
    SDL_QueueAudio(device, samples.data(), samples.size() * sizeof(SampleStereo));
}

bool needs_more_data()
{
    uint32_t queue = SDL_GetQueuedAudioSize(device);
    if (queue < device_spec.size*2)
        return true;
    return false;
}

}
}
