#pragma once

#include "audio/common.hpp"
#include "audio/effect.hpp"
#include "containers/bitmask_ringbuf.hpp"
#include "fast_math/sin.hpp"
#include "fast_math/tanh.hpp"
#include <cmath>

namespace rana::audio {

class Chorus : public Effect {
    static constexpr auto kParamCount = 9;
    const char *kParamNames[kParamCount] = {
        "mix",
        "width",
        "delay",
        "speed",
        "intensity",
        "voices",
        "highpass",
        "lowpass",
        "feedback",
    };
    union {
        float by_index[kParamCount];
        struct {
            float mix       = 0.5;
            float width     = 0.5;
            float delay     = 0.05;
            float speed     = 0.25;
            float intensity = 0.2;
            float voices    = 0.0;
            float highpass  = 0.0;
            float lowpass   = 1.0;
            float feedback  = 0.0;
        };
    } param;

    static constexpr auto kUpdateRateSamples = 4;
    static constexpr auto kRingBufSize = 1024 * 32;

    BitmaskRingBuf<SampleStereo, kRingBufSize> rb = {}; // FIXME: replace with a proper delay line class
    struct Voice {
        SampleStereo pan;
        float phase;
        float offset;
        float speed_mult;
    } voices[8] = {
        { .pan = pan_equal_power(0),   .phase = -1.00, .speed_mult = 1.01f }, 
        { .pan = pan_equal_power(1),   .phase =  0.00, .speed_mult = 0.99f }, 
        { .pan = pan_equal_power(0.1), .phase = -0.75, .speed_mult = 1.02f }, 
        { .pan = pan_equal_power(0.9), .phase =  0.25, .speed_mult = 0.98f }, 
        { .pan = pan_equal_power(0.2), .phase = -0.50, .speed_mult = 1.03f }, 
        { .pan = pan_equal_power(0.8), .phase =  0.50, .speed_mult = 0.97f }, 
        { .pan = pan_equal_power(0.3), .phase = -0.25, .speed_mult = 1.04f }, 
        { .pan = pan_equal_power(0.7), .phase =  0.75, .speed_mult = 0.96f }, 
    };
    SampleStereo lp_coef;
    SampleStereo hp_coef;
    SampleStereo lp = 0;
    SampleStereo hp = 0;
    int update_counter = 0;
    float sr;

public:
    Chorus(float sample_rate = 44100) : sr{sample_rate}
    {
    }

    virtual const char *getName()           { return "Chorus"; }
    virtual void setParam(int i, float v)   { param.by_index[i] = v; }
    virtual float getParam(int i)           { return param.by_index[i]; }
    virtual int getParamCount()             { return kParamCount; }
    virtual const char *getParamName(int i) { return kParamNames[i]; }

    virtual void process(SampleStereo *buf, size_t n) {
        const size_t n_voices = 2 + param.voices * 6;
        const auto gain = SampleStereo(std::sqrt(1.0 / n_voices));
        const auto dry = SampleStereo(sqrt(1.0f - param.mix));
        const auto wet = SampleStereo(sqrt(param.mix));
        const float intensity_scaled = std::pow(param.intensity, 3) * 0.1f;
        const float base_speed = (9.9f * param.speed + 0.1f) / sr;
        const float base_increment = base_speed * 2.0f * kUpdateRateSamples;
        const float delay_samples = param.delay * 0.5 * sr;
        lp_coef = factor_lowpass_single_pole(20.0 + std::pow(param.lowpass, 3) * 22030.0, sr);
        hp_coef = factor_lowpass_single_pole(20.0 + std::pow(param.highpass, 3) * 22030.0, sr);

        for (size_t i = 0; i < n; i++) {
            const auto in = buf[i];
            SampleStereo sum = {};
            SampleStereo sum_unpanned = {};

            if (update_counter == 0) {
                for (size_t voice_idx = 0; voice_idx < n_voices; voice_idx++) {
                    auto &v = voices[voice_idx];

                    float speed = base_speed * v.speed_mult;
                    float increment = base_increment * v.speed_mult;
                    v.phase += increment;
                    if (v.phase >= 1.0f) v.phase -= 2.0f;
                    float voice_delay = delay_samples * (voice_idx + 1) * (1.0f / n_voices);
                    float intensity = std::min<float>(intensity_scaled / speed, kRingBufSize - 2 - voice_delay);
                    v.offset = float(kRingBufSize-1) - (1.0f + fmath::sinpi_f32(v.phase)) / 2.0f * intensity - voice_delay;
                }
                update_counter = kUpdateRateSamples;
            }
            update_counter--;

            for (size_t voice_idx = 0; voice_idx < n_voices; voice_idx++) {
                auto &v = voices[voice_idx];
                const size_t offset_i = v.offset;
                auto frac = SampleStereo(v.offset - offset_i);
                const auto x0 = rb[offset_i];
                const auto x1 = rb[offset_i+1];
                const auto y = (x0 + (x1 - x0) * frac);
                sum_unpanned += y;
                sum += y * v.pan;
            }
            sum *= gain;
            const auto in_fb = in + sum_unpanned * (-param.feedback) * (1.f / n_voices);
            lp += (in_fb - lp) * lp_coef;
            hp += (lp - hp) * hp_coef;
            const auto filtered = lp - hp;
            rb.push(fmath::tanh(filtered));

            SampleStereo ms = { sum.l + sum.r, sum.l - sum.r };
            ms.r *= param.width * 2.0;
            sum = { (ms.l + ms.r) * 0.5, (ms.l - ms.r) * 0.5 };
            buf[i] = dry * in + sum * wet;
        }
    }
};

} // namespace rana::audio
