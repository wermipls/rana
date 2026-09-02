#pragma once

#include "audio/effect.hpp"
#include <cmath>
#include <algorithm>
#include <tracy/Tracy.hpp>
#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    #include <stdio.h>
#endif

namespace rana::audio {

using std::pow, std::min, std::max;

class SinglePole : public Effect {
protected:
    static constexpr auto paramCount = 1;
    const char *paramNames[paramCount] = {
        "cutoff",
    };
    float params[paramCount] = { 0.5 };

    Hz sr;
    SampleStereo coeff = 0;
    SampleStereo q{};

    inline float normalized2coeff(float value)
    {
        // we want the filter value to reach 1 on the extreme edge
        // so you can set it so it doesn't affect the sound when doing LP,
        // rather than be truly accurate to -3dB frequency
        float coeff = factor_lowpass_single_pole(pow(value, 2) * 19980 + 20, sr);
        return min(coeff + pow(value, 30.0f), 1.0f);
    }

    SinglePole(Hz sample_rate = 44100)
    {
        sr = sample_rate;
        setCutoff(3000);
    }
    void setCutoff(Hz freq)
    {
        coeff = factor_lowpass_single_pole(freq, sr);
    }

public:
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    void getParamFmt(int index, char *str, size_t sz) {
        auto hz = std::acos(1 - coeff.l*coeff.l / (2-2*coeff.l)) * sr / (2 * pi);
        if (std::isnormal(hz)) {
            snprintf(str, sz, "%f", hz);
        } else {
            snprintf(str, sz, "> %f", sr/2);
        }
    }
    static const char *getParamLabel(int index) { return "Hz"; }
#endif

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: coeff = normalized2coeff(value); break;
        }
    }
};

class Lowpass : public SinglePole {
public:
    Lowpass(Hz sample_rate = 44100) : SinglePole(sample_rate) {}
    virtual const char *getName() { return "Lowpass"; }
    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Lowpass");
        for (size_t i = 0; i < n; i++) {
            q += (in[i] - q) * coeff;
            in[i] = q;
        }
    }
};

class Highpass : public SinglePole {
public:
    Highpass(Hz sample_rate = 44100) : SinglePole(sample_rate) {}
    virtual const char *getName() { return "Highpass"; }
    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Highpass");
        for (size_t i = 0; i < n; i++) {
            q += (in[i] - q) * coeff;
            in[i] -= q;
        }
    }
};

} // namespace rana::audio
