#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <cstring>
#include <SDL2/SDL.h>
#include "common.hpp"
#include "effects.hpp"
#include "log.hpp"

namespace rana {
namespace audio {

constexpr Hz RnsVolumeSmoothing = 27.5;
constexpr Hz RnsDeclickSmoothing = 110;

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

class Generator {
protected:
    float sr = 44100;
    float freq = 440;
    float volume = 1.0;
    float pan = 0.0;
public:
    virtual std::vector<SampleStereo> getSamples(size_t n_samples) = 0;
    virtual void setVolume(float volume) = 0;
    virtual void setFrequency(Hz freq) = 0;
    virtual void setPan(float pan) = 0;
    virtual ~Generator() = default;
};

class Sine : public Generator {
    float t = 0;
    float volume_target;
    float smoothing_factor;
    float smoothing_periods;
    SampleStereo pan_factors;
    bool is_eqp;

public:
    Sine(Hz sample_rate, Hz freq = 440, float volume = 1.0, float smoothing_periods = 4.0, bool equal_power = true) {
        sr = sample_rate;
        this->smoothing_periods = smoothing_periods;
        this->volume = 0;
        volume_target = volume;
        setFrequency(freq);
        setPan(0);
    }

    void updateSmoothingFactor()
    {
        auto freq_normalized = normalize_frequency(freq / smoothing_periods, sr);
        smoothing_factor = derive_1pole_factor(freq_normalized);
    }

    void updateVolume()
    {
        volume += (volume_target - volume) * smoothing_factor; 
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        auto buf = std::vector<SampleStereo>(n_samples);

        float pt = t;

        for (auto &a : buf) {
            updateVolume();
            t = std::fmod(t + freq / sr, 1.0f);
            a.l = a.r = (sin(t * M_PI*2.) + sin(pt * M_PI*2.)) / 2.f * volume;
            a.l *= pan_factors.l;
            a.r *= pan_factors.r;
            pt = t;
        }

        return buf;
    }

    void setVolume(float volume) { this->volume_target = volume; }
    void setFrequency(Hz freq) {
        this->freq = freq;
        updateSmoothingFactor();
    }

    void setPan(float pan)
    {
        if (is_eqp) {
            this->pan_factors = pan_equal_power(pan);
        } else {
            if (pan < 0) {
                pan_factors.l = 1; 
                pan_factors.r = 1.f + pan;
            } else {
                pan_factors.l = 1.f - pan;
                pan_factors.r = 1; 
            }
        }
    }
};

class Saw : public Generator {
    float t = 0;
    float volume_target;
    float smoothing_factor;
    float smoothing_periods;
    SampleStereo pan_factors;
    bool is_eqp;

public:
    Saw(Hz sample_rate, Hz freq = 440, float volume = 1.0, float smoothing_periods = 4.0, bool equal_power = true) {
        sr = sample_rate;
        this->smoothing_periods = smoothing_periods;
        this->volume = 0;
        volume_target = volume;
        setFrequency(freq);
        setPan(0);
    }

    void updateSmoothingFactor()
    {
        smoothing_factor = factor_1pole(RnsVolumeSmoothing, sr);
    }

    void updateVolume()
    {
        volume += (volume_target - volume) * smoothing_factor; 
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        auto buf = std::vector<SampleStereo>(n_samples);

        float pt = t;

        for (auto &a : buf) {
            updateVolume();
            t = std::fmod(t + freq / sr, 1.0f);
            a.l = a.r = (t + pt - 1.f) * volume;
            a.l *= pan_factors.l;
            a.r *= pan_factors.r;
            pt = t;
        }

        return buf;
    }

    void setVolume(float volume) { this->volume_target = volume; }
    void setFrequency(Hz freq) {
        this->freq = freq;
        updateSmoothingFactor();
    }

    void setPan(float pan)
    {
        if (is_eqp) {
            this->pan_factors = pan_equal_power(pan);
        } else {
            if (pan < 0) {
                pan_factors.l = 1; 
                pan_factors.r = 1.f + pan;
            } else {
                pan_factors.l = 1.f - pan;
                pan_factors.r = 1; 
            }
        }
    }
};

}
}
