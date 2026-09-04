#pragma once

#include "audio/common.hpp"
#include "audio/effect.hpp"

namespace rana::audio {

class Gain : public Effect {
protected:
    static constexpr auto paramCount = 2;
    const char *paramNames[paramCount] = {
        "gain",
        "pan",
    };
    float params[paramCount] = { 0.75, 0.5 };

    Rampable gain;
    Rampable pan;

public:
    Gain(Hz sample_rate = 44100)
        : gain(1.0, sample_rate)
        , pan(0.5, sample_rate)
    {}

    virtual const char *getName() { return "Gain"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    void getParamFmt(int index, char *str, size_t sz) {
        switch (index) {
            case 0: snprintf(str, sz, "%.2f", 20.0f * std::log10(gain.target)); break;
            case 1: {
                const auto p = pan.target * 2.0f - 1.0f;
                if (p != 0.0) {
                    snprintf(str, sz, p > 0.0f ? "%.2f%% R" : "%.2f%% L", std::abs(p) * 100.0f);
                } else {
                    snprintf(str, sz, "center");
                }
                break;
            }
        }
    }

    static const char *getParamLabel(int index) {
        const char *labels[] = {
            "dB",
            "",
        };
        static_assert(std::size(labels) == paramCount);
        return labels[index % paramCount];
    }
#endif

    virtual void setParam(int index, float value) {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: gain = value ? std::pow(10, (value * 80.0f - 60.0f) / 20.0f) : 0.0f; break;
            case 1: pan = value; break;
        }
    }

    virtual void process(SampleStereo *in, size_t n) {
        for (size_t i = 0; i < n; i++) {
            const auto gain_cur = SampleStereo(gain());
            const auto pan_cur = pan();
            in[i] *= gain_cur * pan_equal_power(pan_cur);
        }
    }
};

} // namespace rana::audio
