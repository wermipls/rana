#pragma once

#include "audio/effect.hpp"
#include <cmath>
#include <tracy/Tracy.hpp>

namespace rana::audio {

class Compressor : public Effect {
    static constexpr auto paramCount = 5;
    const char *paramNames[paramCount] = {
        "threshold",
        "attack",
        "release",
        "ratio",
        "makeup",
    };
    float params[paramCount] = {
        0.8,
        0.2,
        0.5,
        0.5,
        0.0,
    };

    static constexpr float PeakSmoothingHz = 20.0f;
    double sr;
    double pp_coeff;
    double max = 0;
    double pp = 0;
    double attack_coeff = 0.0;
    double release_coeff = 0.0;
    double threshold_db = 0.8;
    double volume_actual = 1.0;
    double volume_target = 0;
    double makeup = 1;
    double ratio = 0.5;

    void peakToPeak(SampleStereo value)
    {
        max *= pp_coeff;
        auto value_abs = abs(value);
        max = std::max(max, value_abs.l);
        max = std::max(max, value_abs.r);
        pp = dB(std::abs(max));
    }

    void processVolume()
    {
        if (volume_actual > volume_target) {
            volume_actual += (volume_target - volume_actual) * attack_coeff;
        } else {
            volume_actual += (volume_target - volume_actual) * release_coeff;
        }
    }

    static inline float dB(float volume)
    {
        return 20 * std::log10(volume);
    }

    static inline float from_dB(float a)
    {
        return std::pow(10, a/20);
    }

public:
    Compressor(float sample_rate = 44100) : sr{sample_rate}
    {
        pp_coeff = 1.0f - factor_lowpass_single_pole(PeakSmoothingHz, sr);
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: threshold_db = dB(std::pow(value, 3) * 0.999f + 0.001f); break;
            case 1: attack_coeff  = factor_lowpass_single_pole(1.0f + std::pow(1.0f - value, 10) * 22049.0f, sr); break;
            case 2: release_coeff = factor_lowpass_single_pole(0.1f + std::pow(1.0f - value, 10) * 999.9f, sr); break;
            case 3: ratio = value; break; 
            case 4: makeup = std::pow(value, 3) * 16.0f + 1.0f; break;
        }
    }

    virtual const char *getName() { return "Compressor"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

    virtual void process(SampleStereo *in, size_t n)
    {
        // FIXME: vectorize.
        ZoneScopedN("Compressor");
        for (size_t i = 0; i < n; i++) {
            peakToPeak(in[i]);
            auto delta_db = pp - threshold_db;
            auto target_db = 0;
            if (delta_db > 0.0f) target_db -= delta_db * ratio;
            volume_target = from_dB(target_db);
            processVolume();
            in[i] *= volume_actual * makeup;
        }
    }
};

} // namespace rana::audio
