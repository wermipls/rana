#pragma once

#include "audio/effect.hpp"
#include <algorithm>
#include <tracy/Tracy.hpp>
#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    #include <stdio.h>
#endif

namespace rana::audio {

using std::min, std::max;

class TranceGate : public Effect {
    static constexpr auto paramCount = 5;
    const char *paramNames[paramCount] = {
        "dry/wet",
        "attack",
        "decay",
        "sustain",
        "release",
    };
    float params[paramCount] = {};

    float sr;
    float bpm = 120;

    float mix = 1;
    float attack  = 0.005;
    float decay   = 0.2;
    float sustain = 0.5;
    float release = 0.02;

    enum Note : uint8_t {
        Off    = 0,
        On     = 1,
        Legato = 2,
    };

    uint8_t pattern[64] = { 1,0,1,2,0,1,1,0, 1,0,1,2,0,1,1,0 };
    int pattern_length = 16;
    int pattern_pos = 0;
    float measure_length = 1;
    float samples_until_next = 0;

    bool is_release = true;
    float last = 0;
    float time_since_trigger = 0;
    float last_release = 0;
    float time_since_release = 0;

    void note()
    {
        switch (pattern[pattern_pos]) {
            case Note::On:
                time_since_trigger = 0;
                if (!is_release) {
                    time_since_release = 0;
                    last_release = last;
                }
                is_release = false;
                break;
            case Note::Legato:
                is_release = false;
                break;
            case Note::Off:
                if (!is_release) {
                    time_since_release = 0;
                    last_release = last;
                }
                is_release = true;
                break;
        }
        pattern_pos = (pattern_pos + 1) % pattern_length;
        samples_until_next = 60.0f / bpm * 4.0f * measure_length / (float)pattern_length * sr; // FIXME: actually calculate fragment time
    }

public:
    TranceGate(float sample_rate = 44100)
    {
        sr = sample_rate;
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: mix = value; break;
            case 1: attack = 0.001f + std::pow(value, 3) * 2.0f; break;
            case 2: decay = 0.001f + std::pow(value, 3) * 2.0f; break;
            case 3: sustain = value; break;
            case 4: release = 0.001f + std::pow(value, 3) * 2.0f; break;
        }
    }

    virtual const char *getName() { return "TranceGate"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

    virtual void process(SampleStereo *in, size_t n)
    {
        for (size_t i = 0; i < n; i++) {
            if (samples_until_next <= 0.0f) {
                note();
            }

            float volume;
            float volume_release = (1.0f - min(1.0f, time_since_release / release)) * last_release;
            // adsr
            if (!is_release) {
                if (time_since_trigger <= attack) {
                    volume = time_since_trigger / attack;
                } else {
                    volume = 1.0f - min(1.0f, ((time_since_trigger - attack) / decay)) * (1.0f - sustain); 
                }
                last = volume;
                volume = max(volume, volume_release);
            } else {
                volume = volume_release;
            }

            volume = volume * volume;

            samples_until_next--;
            time_since_trigger += 1.0f / sr;
            time_since_release += 1.0f / sr;
            in[i] *= volume;
        }
    }
};

} // namespace rana::audio
