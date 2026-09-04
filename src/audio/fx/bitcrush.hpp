#pragma once

#include "audio/effect.hpp"
#include <algorithm>
#include <cmath>
#include <tracy/Tracy.hpp>
#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    #include <stdio.h>
#endif

namespace rana::audio {

class Bitcrush : public Effect {
    static constexpr auto paramCount = 3;
    const char *paramNames[paramCount] = {
        "bits",
        "rate",
        "smoothing",
    };
    float params[paramCount] = {
        1,
        1,
        0,
    };

    int bits = 16;
    double sr, rate;
    double t = 0;
    double smoothing = 0;
    SampleStereo x0 = {0,0};
    SampleStereo x1 = {0,0};

    inline static int bitcrush(int a, int bits)
    {
        int shift = (16 - bits);
        a = a >> shift;
        if (a < 0) a++; // compensate for two's complement
        return a << shift;
    }

    inline static int clamp(int a, int min, int max)
    {
        if (a < min) return min;
        if (a > max) return max;
        return a;
    }

    inline static int float2int(float a, int min, int max)
    {
        float mul = max - min;
        a *= mul;
        a += min;
        return clamp(a, min, max);
    }

public:
    Bitcrush(float sample_rate = 44100) : sr{sample_rate}
    {
        setRate(1);
    }

    void setBits(float value)
    {
        bits = float2int(value, 2, 16);
    }

    void setRate(double value)
    {
        rate = 44100.0 * std::pow(value, 2);
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: setBits(value); break;
            case 1: setRate(value); break;
            case 2: smoothing = value; break;
        }
    }

    virtual const char *getName() { return "Bitcrush"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    void getParamFmt(int index, char *str, size_t sz) {
        switch (index) {
            case 0: snprintf(str, sz, "%d", bits); break;
            case 1: snprintf(str, sz, "%f", rate); break;
            case 2: snprintf(str, sz, "%f", smoothing); break;
        }
    }
    static const char *getParamLabel(int index) {
        static const char *labels[] = {
            "bits",
            "Hz",
            "",
        };
        static_assert(std::size(labels) == paramCount);
        return labels[index % paramCount];
    }
#endif

    virtual void process(SampleStereo *in, size_t n)
    {
        // FIXME: vectorize.
        ZoneScopedN("Bitcrush");
        double linear_amt = std::max(smoothing*2.0 - 1.0, 0.0);
        double shave_amt = std::min(smoothing*2.0, 1.0);
        for (size_t i = 0; i < n; i++) {
            // rate
            t += rate / sr;
            double s = 0;
            if (t >= 1.0) {
                t -= 1.0;
                s = t;
                x1 = x0;
                x0 = in[i];
            }

            // bitcrush
            int l0 = bitcrush(x0.l * 32768, bits);
            int r0 = bitcrush(x0.r * 32768, bits);
            int l1 = bitcrush(x1.l * 32768, bits);
            int r1 = bitcrush(x1.r * 32768, bits);

            s = 1.f - s * shave_amt;
            s = s * (1.f - linear_amt) + t * linear_amt;

            in[i].l = (l0 / 32768.0) * s + (l1 / 32768.0) * (1.0 - s);
            in[i].r = (r0 / 32768.0) * s + (r1 / 32768.0) * (1.0 - s);
        }
    }
};

} // namespace rana::audio
