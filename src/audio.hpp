#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <cstring>
#include <SDL2/SDL.h>
#include "audio_common.hpp"
#include "audio_effects.hpp"

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

class Sample {
protected:
    bool looped = false;
    bool reverse = false;
    size_t loop_start = 0;
    size_t loop_end = 0;
    size_t position = 0;

public:
    virtual std::vector<SampleStereo> getSamples(size_t n_samples) = 0;
    virtual void seek(float pos) = 0;
    virtual void setLooping(bool is_looping) = 0;
    virtual void setReverse(bool reversed) = 0;
    virtual double getSampleRate() = 0;
};

class Track {
    std::vector<Generator *> sources;

public:
    void addSource(Generator *source)
    {
        sources.push_back(source);
    }

    void removeSource(Generator *source)
    {
        for (size_t i = 0; i < sources.size(); i++) {
            if (sources[i] == source) {
                sources.erase(sources.begin() + i);
            }
        }
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        std::vector<SampleStereo> mix(n_samples);

        for (auto &src : sources) {
            auto samples = src->getSamples(n_samples);
            for (size_t i = 0; i < n_samples; i++) {
                mix[i].l += samples[i].l;
                mix[i].r += samples[i].r;
            }
        }

        return mix;
    }
};

class SampleMonoS16 : public Sample {
    std::vector<int16_t> data;
    Hz sr;

public:
    SampleMonoS16(std::vector<int16_t> data, Hz sample_rate)
    {
        this->data = data;
        loop_end = data.size() - 1;
        sr = sample_rate;
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        std::vector<SampleStereo> buf(n_samples);

        for (auto &s : buf) {
            if (position >= data.size()) {
                continue;
            }
            s.l = s.r = data[position] / (float)INT16_MAX;

            if (!reverse) {
                position++;
                if (looped && position >= loop_end) {
                    position = loop_start;
                }
            } else {
                position--;
                if (looped && position <= loop_start) {
                    position = loop_end;
                }
            }
        }

        return buf;
    }

    void seek(float pos)
    {
        pos = std::fminf(std::fmaxf(pos, 0.0), 1.0);
        position = pos * (data.size()-1);
    }

    void setLooping(bool is_looping) { looped = is_looping; };
    void setReverse(bool reversed) { reverse = reversed; };
    Hz getSampleRate() { return sr; }
};

Sample *load_sample(std::string path)
{
    // fixme: no error handling...
    SDL_AudioSpec spec;
    uint8_t *buf;
    uint32_t len;

    SDL_LoadWAV(path.c_str(), &spec, &buf, &len);

    if (spec.format == AUDIO_S16 && spec.channels == 1) {
        std::vector<int16_t> data(len / sizeof(int16_t));
        std::memcpy(data.data(), buf, len);
        SDL_FreeWAV(buf);
        return new SampleMonoS16(data, spec.freq);
    } else {
        SDL_FreeWAV(buf);
    }
    return nullptr;
}


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

enum InterpolationMethod {
    None,
    Hybrid,
    Linear,
};

class Sampler : Generator {
    float t = 0;
    float volume_target;
    float smoothing_factor;
    SampleStereo pan_factors;
    bool is_eqp;
    SampleStereo prev_sample{};
    std::vector<SampleStereo> buffer;
    size_t buffer_pos = 0;
    Sample *sample;
    size_t loop_start = 0;
    size_t loop_end = 0;
    InterpolationMethod interpolation;

    void updateVolume()
    {
        volume += (volume_target - volume) * smoothing_factor; 
    }

public:
    Sampler(Sample *sample, Hz freq = 440, float volume = 1.0, bool equal_power = true, InterpolationMethod interpolation = Hybrid) {
        this->sample = sample;
        sr = sample->getSampleRate();
        this->volume = 0.f;
        volume_target = volume;
        this->interpolation = interpolation;
        setFrequency(freq);
        setPan(0);

        smoothing_factor = factor_1pole(RnsVolumeSmoothing, sr);
    }

    std::vector<SampleStereo> getSamples(size_t n_samples)
    {
        auto buf = std::vector<SampleStereo>(n_samples);

        for (auto &a : buf) {
            updateVolume();
            while (buffer_pos >= buffer.size()) {
                buffer_pos -= buffer.size();
                buffer = sample->getSamples(64);
            }
            auto smp = buffer[buffer_pos];
            float tt = t;
            if (interpolation == None) {
                tt = 0;
            }
            a.l = smp.l * tt + prev_sample.l * (1.f - tt);
            a.r = smp.r * tt + prev_sample.r * (1.f - tt);
            a.l *= volume * pan_factors.l;
            a.r *= volume * pan_factors.r;
            if (interpolation != Linear) {
                prev_sample = smp;
            }

            t += freq / 440.0f;
            while (t > 1.f) {
                t -= 1.f;
                buffer_pos++;
                if (interpolation == Linear) {
                    prev_sample = smp;
                }
            }
        }

        return buf;
    }

    void setVolume(float volume) { this->volume_target = volume; }
    void setFrequency(Hz freq) { this->freq = freq; }

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

class Player
{
    double bpm;
    double ticks;
    Hz sample_rate;

};


}
}
