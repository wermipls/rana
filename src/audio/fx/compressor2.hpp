#pragma once

#include "audio/effect.hpp"
#include "containers/bitmask_ringbuf.hpp"
#include "fast_math/log.hpp"
#include "fast_math/pow.hpp"
#include <algorithm>
#include <cmath>
#include <tracy/Tracy.hpp>
#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    #include <stdio.h>
#endif

namespace rana::audio {

using std::abs, std::min, std::max;

class Compressor2 : public Effect {
    static constexpr auto paramCount = 8;
    const char *paramNames[paramCount] = {
        "threshold",
        "attack",
        "release",
        "ratio",
        "knee",
        "makeup",
        "lookahead",
        "auto makeup",
    };
    float params[paramCount] = {
        0.8,
        0.2,
        0.5,
        0.5,
        0.1,
        0.0,
        0.0,
        0.0,
    };

    // for display only.
    double attack_time;
    double release_time;
    double lookahead_time;
    double makeup_db;

    static constexpr float PeakSmoothingHz = 20.0f;
    static constexpr double MaxLookaheadSeconds = 0.020;
    static constexpr size_t MaxLookaheadSamples = 192000 * MaxLookaheadSeconds; // assume 192k is max for the effect.
    double sr;
    double pp_coeff;
    double pp_lookahead_coeff = 1;
    double pp = 0;
    double attack_coeff = 0;
    double release_coeff = 0;
    double threshold_db = 0.8;
    double volume_actual = 1.0;
    double volume_target = 0;
    double makeup = 1;
    double ratio = 0.5;
    double knee = 0.01;
    double auto_makeup;
    size_t lookahead_samples = 0;

    BitmaskRingBuf<SampleStereo, MaxLookaheadSamples> delay_buf = {};
    BitmaskRingBuf<double, MaxLookaheadSamples> peak_buf = {};

    void peakToPeak(SampleStereo value)
    {
        pp *= pp_coeff;
        auto value_abs = abs(value);
        auto max = pp;
        max = std::max(max, value_abs.l);
        max = std::max(max, value_abs.r);
        pp = max;
    }

    double attack_ramp()
    {
        double peak = peak_buf[MaxLookaheadSamples-1];
        for (size_t i = MaxLookaheadSamples-1; i > MaxLookaheadSamples - lookahead_samples; i--) {
            peak *= pp_lookahead_coeff;
            const auto prev_peak = peak_buf[i-1];
            if (prev_peak > peak) {
                peak_buf[i-1] = prev_peak;
                break;
            }
            peak_buf[i-1] = peak;
        }
        auto pb = peak_buf[MaxLookaheadSamples - lookahead_samples - 1];
        return pb;
    }

    void processVolume()
    {
        if (volume_actual > volume_target) {
            volume_actual += (volume_target - volume_actual) * attack_coeff;
        } else {
            volume_actual += (volume_target - volume_actual) * release_coeff;
        }
    }

    static inline double dB(double volume)
    {
        return 20.0 * fmath::log10(volume);
    }

    static inline double from_dB(double a)
    {
        return fmath::pow(10, a/20);
    }

    static inline double factor_single_pole_target(double target, double iterations)
    {
        static_assert(std::numeric_limits<double>::is_iec559); // we need div by zero to yield +inf.
        return std::pow(target, 1.0 / iterations);
    }

    static inline double smooth_min(double a, double b, double k)
    {
        auto x = max(k - abs(a - b), 0.0);
        return min(a, b) - x * x / (k * 4.0);
    }

public:
    Compressor2(float sample_rate = 44100) : sr{sample_rate}
    {
        pp_coeff = 1.0f - factor_lowpass_single_pole(PeakSmoothingHz, sr);
        pp_lookahead_coeff = 0;
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: threshold_db = value * 60.0 - 60.0; break;
            case 1: {
                attack_time = std::pow(value, 3) * 10.0;
                attack_coeff  = 1.0 - factor_single_pole_target(from_dB(-30), sr * attack_time);
                break;
            }
            case 2: {
                release_time = std::pow(value, 3) * 10.0;
                release_coeff = 1.0 - factor_single_pole_target(from_dB(-30), sr * release_time);
                break;
            }
            case 3: ratio = value * 1.25 - 0.25; break; 
            case 4: knee = value * (30.0 - 0.01) + 0.01; break;
            case 5: {
                makeup_db = value * 60.0;
                makeup = from_dB(makeup_db);
                break;
            }
            case 6: {
                lookahead_time = value * MaxLookaheadSeconds;
                lookahead_samples = lookahead_time * sr;
                pp_lookahead_coeff = factor_single_pole_target(from_dB(-20.0), lookahead_samples);
                break;
            }
            case 7: auto_makeup = value; break;
        }
    }

    virtual const char *getName() { return "Compressor2"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    void getParamFmt(int index, char *str, size_t sz)
    {
        switch (index) {
            case 0: snprintf(str, sz, "%f", threshold_db); break;
            case 1: snprintf(str, sz, "%f", attack_time * 1000.0); break;
            case 2: snprintf(str, sz, "%f", release_time * 1000.0); break;
            case 3: snprintf(str, sz, "%f", ratio); break;
            case 4: snprintf(str, sz, "%f", params[4]); break;
            case 5: snprintf(str, sz, "%f", makeup_db); break;
            case 6: snprintf(str, sz, "%f", lookahead_time * 1000.0); break;
            case 7: snprintf(str, sz, "%f", auto_makeup); break;
        }
    }

    static const char *getParamLabel(int index)
    {
        static const char *labels[] = {
            "dB",
            "ms",
            "ms",
            "",
            "",
            "dB",
            "ms",
            "",
        };
        static_assert(std::size(labels) == paramCount);
        return labels[index % paramCount];
    }
#endif

    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Compressor2");
        auto makeup_actual = makeup * from_dB(-threshold_db * auto_makeup);
        for (size_t i = 0; i < n; i++) {
            auto s = in[i];
            delay_buf.push(s);
            peakToPeak(s);
            peak_buf.push(pp);
            auto pp_db = dB(attack_ramp());
            auto delta_db = pp_db - threshold_db;
            auto comp_curve = smooth_min(threshold_db + delta_db * ratio, pp_db, knee);
            volume_target = from_dB(comp_curve - pp_db);
            // may be nan if peak was 0, so just substitute with a sane value.
            if (std::isnan(volume_target)) {
                volume_target = 1;
            }
            processVolume();
            in[i] = delay_buf[MaxLookaheadSamples - lookahead_samples - 1] * volume_actual * makeup_actual;
        }
    }
};

} // namespace rana::audio
