#pragma once

#include "audio/effect.hpp"
#include "audio/halfband.hpp"
#include "fast_math/sin.hpp"
#include <cmath>
#include <span>
#include <tracy/Tracy.hpp>
#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    #include <stdio.h>
#endif

namespace rana::audio {

class Distortion : public Effect {
    static constexpr auto paramCount = 6;
    const char *paramNames[paramCount] = {
        "amount",
        "mix",
        "mode",
        "asymmetry",
        "stereo bias",
        "oversample",
    };
    float params[paramCount] = {
        0.0,
        1.0,
        0.0,
        0.5,
        0.5,
        0.0,
    };

    static constexpr float gain_multi = 127;
    enum Mode {
        Hardclip,
        Softclip,
        Softsine,
        Tanh,
        Shape,
        Fold,
        BadFold,
    } mode = Hardclip;
    double gain = 1;
    double asymmetry = 0.5;
    double stereo_bias = 0;

    Oversampler2x<SampleStereo> oversampler = {};

    static SampleStereo bias(SampleStereo x, SampleStereo offset) {
        // this is the best way i can think of for now.
        const auto sign = copysign(1.0, x);
        const auto pos = max(sign, 0.0);
        const auto neg = SampleStereo(1.0) - pos;
        return x * pos * 2.0 * (SampleStereo(1.0) - offset)
             + x * neg * 2.0 * offset;
    }

public:
    Distortion()
    {
        mode = Hardclip;
        oversampler.enable(false);
    }

    void setAmount(float value)
    {
        gain = std::pow(value, 3) * gain_multi + 1;
    }

    void setMix(float value)
    {
        oversampler.mix(value);
    }

    void setMode(float value)
    {
        int i = value * (float)BadFold;
        mode = (Mode)i;
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: setAmount(value); break;
            case 1: setMix(value); break;
            case 2: setMode(value); break;
            case 3: asymmetry = value; break;
            case 4: stereo_bias = value - 0.5; break;
            case 5: oversampler.enable(value >= 0.5); break;
        }
    }

    virtual const char *getName() { return "Distortion"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    void getParamFmt(int index, char *str, size_t sz) {
        const char *mode_str[] = {
            "Hardclip",
            "Softclip",
            "Softsine",
            "Tanh",
            "Shape",
            "Fold",
            "BadFold"
        };
        switch (index) {
            default: snprintf(str, sz, "%f", params[index]); break;
            case 0: snprintf(str, sz, "%f", gain); break;
            case 2: snprintf(str, sz, "%s", mode_str[mode]); break;
            case 3: snprintf(str, sz, "%f", asymmetry - 0.5); break;
            case 4: snprintf(str, sz, "%f", stereo_bias); break;
            case 5: snprintf(str, sz, "%s", oversampler.enabled() ? "2x" : "none"); break;
        }
    }
    static const char *getParamLabel(int index) { return ""; }
#endif

    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Distortion");
        const auto range = std::span(in, in+n);
        // it can go below 0, but that's OK actually.
        const auto offset = SampleStereo(asymmetry - stereo_bias, asymmetry + stereo_bias);

        switch (mode) 
        {
        case Hardclip:
            oversampler.for_each(range, [this, offset](auto smp) {
                smp = bias(smp, offset);
                return max(min(smp * gain, 1.0), -1.0);
            });
            break;
        case Softclip: {
            oversampler.for_each(range, [this, offset](auto smp) {
                constexpr auto a = -1.42479f;
                constexpr auto b =  2.20888f;
                constexpr auto c = -1.02786f;
                constexpr auto d =  1.13379f;
                smp = bias(smp, offset);
                auto x = min(1.0, abs(smp) * gain);
                return copysign((x*x*x*x*a + x*x*x*b + x*x*c + x*d), smp);
            });
            break;
        }
        case Softsine:
            oversampler.for_each(range, [this, offset](auto smp) {
                smp = bias(smp, offset);
                auto x = min(1.0f, max(-1.0, smp * gain / sqrt2));
                return fmath::sin_halfpi(x);
            });
            break;
        case Tanh:
            oversampler.for_each(range, [this, offset](auto smp) {
                smp = bias(smp, offset);
                return fast_tanh(smp * gain);
            });
            break;
        case Shape:
            oversampler.for_each(range, [this, offset](auto smp) {
                auto x = bias(smp, offset);
                auto xabs = abs(x);
                auto p = SampleStereo(1.0) / (gain); 
                auto curve = xabs / (xabs - p * (xabs - 1));
                return copysign(curve, x);
            });
            break;
        case Fold:
            oversampler.for_each(range, [this, offset](auto smp) {
                smp = bias(smp, offset);
                auto a = (smp * gain - 1.0) / 4.0;
                return abs(a - floor(a) - 0.5) * 4.0 - 1.0;
            });
            break;
        case BadFold:
            oversampler.for_each(range, [this, offset](auto smp) {
                smp = bias(smp, offset);
                return fmod(smp * gain, 1.0);
            });
            break;
        }
    }
};

} // namespace rana::audio
