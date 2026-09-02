#pragma once

#include "audio/effect.hpp"
#include <memory>
#include <algorithm>
#include <tracy/Tracy.hpp>
#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    #include <stdio.h>
#endif

namespace rana::audio {

class Delay : public Effect {
    static constexpr auto paramCount = 4;
    const char *paramNames[paramCount] = {
        "wet",
        "dry",
        "feedback",
        "delay",
    };
    float params[paramCount] = {
        0.25,
        1.0,
        0.25,
        0.1,
    };

    static constexpr auto max_delay_seconds = 5.0;
    std::unique_ptr<SampleStereo[]> buffer;
    size_t buffer_size;
    size_t buffer_pos = 0;
    size_t delay_size = 0;
    SampleStereo feedback = 0.25;
    SampleStereo wet = 0.25;
    SampleStereo dry = 1.0;

public:
    Delay(Hz sample_rate = 44100)
    {
        buffer_size = std::max<size_t>(sample_rate * max_delay_seconds, 1);
        buffer = std::make_unique<SampleStereo[]>(buffer_size);

        setDelay(0.2);
    }

    void setDelay(float value)
    {
        assert(value >= 0.0);
        assert(value <= 1.0);

        delay_size = std::max<size_t>(buffer_size * value, 1);
        buffer_pos = buffer_pos % delay_size;
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: wet = value; break;
            case 1: dry = value; break;
            case 2: feedback = value; break;
            case 3: setDelay(value); break;
        }
    }

    virtual const char *getName() { return "Delay"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    void getParamFmt(int index, char *str, size_t sz) {
        switch (index) {
            default: snprintf(str, sz, "%f", params[index]); break;
            case 3: snprintf(str, sz, "%f", params[3] * max_delay_seconds * 1000.0); break;
        }
    }
    static const char *getParamLabel(int index) {
        static const char *labels[] = {
            "",
            "",
            "",
            "ms",
        };
        static_assert(_countof(labels) == paramCount);
        return labels[index % paramCount];
    }
#endif

    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Delay");
        for (size_t i = 0; i < n; i++) {
            buffer_pos++;
            buffer_pos = buffer_pos % delay_size;
            auto delay_sample = buffer[buffer_pos];
            buffer[buffer_pos] = in[i] + delay_sample * feedback;
            in[i] = in[i] * dry + delay_sample * wet;
        }
    }
};

} // namespace rana::audio
