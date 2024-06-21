#include "audio_common.hpp"
#include "freeverb/freeverb.h"
#include <vector>

namespace rana {
namespace audio {

class Effect {
public:
    virtual void process(SampleStereo *in, size_t n) = 0;
    virtual void setParam(int index, float value) = 0;
    virtual ~Effect() = default;
};

class Filter1Pole : public Effect {
    Hz sr;
    float coeff = 0;
    bool highpass;
    SampleStereo q{};

public:
    Filter1Pole(Hz sample_rate = 44100, bool is_highpass = false)
    {
        sr = sample_rate;
        highpass = is_highpass;
        setCutoff(3000);
    }

    void setCutoff(Hz freq)
    {
        coeff = factor_1pole(freq, sr);
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: setCutoff(std::pow(value, 2) * 19980 + 20); break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        if (highpass) {
            for (size_t i = 0; i < n; i++) {
                q.l += (in[i].l - q.l) * coeff;
                q.r += (in[i].r - q.r) * coeff;
                in[i].l -= q.l;
                in[i].r -= q.r;
            }
        } else {
            for (size_t i = 0; i < n; i++) {
                q.l += (in[i].l - q.l) * coeff;
                q.r += (in[i].r - q.r) * coeff;
                in[i] = q;
            }
        }
    }
};

class Reverb : public Effect {
    fv_Context ctx;

public:
    Reverb()
    {
        fv_init(&ctx);
        fv_set_samplerate(&ctx, 44100);
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: fv_set_wet(&ctx, value); break;
            case 1: fv_set_dry(&ctx, value); break;
            case 2: fv_set_width(&ctx, value); break;
            case 3: fv_set_roomsize(&ctx, value); break;
            case 4: fv_set_damp(&ctx, value); break;
            case 5: fv_set_lowpass(&ctx, value); break;
            case 6: fv_set_highpass(&ctx, value); break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        fv_process(&ctx, &in->l, n*2);
    }
};

class Delay : public Effect {
    static constexpr auto max_delay_seconds = 5.0;
    std::vector<SampleStereo> buffer;
    size_t buffer_pos = 0;
    size_t delay_size = 0;
    float feedback = 0.25;
    float wet = 0.25;
    float dry = 1.0;

public:
    Delay(Hz sample_rate = 44100)
    {
        buffer.resize(sample_rate * max_delay_seconds);
        for (auto &n : buffer) {
            n.l = 0;
            n.r = 0;
        }

        setDelay(0.2);
    }

    void setDelay(float value)
    {
        if (value > 1.0) value = 1.0;
        if (value < 0.0) value = 0.0;

        delay_size = buffer.size() * value;
        if (delay_size == 0) delay_size = 1;
        if (delay_size > buffer.size()) delay_size = buffer.size();

        buffer_pos = buffer_pos % delay_size;
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: wet = value; break;
            case 1: dry = value; break;
            case 2: feedback = value; break;
            case 3: setDelay(value); break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        for (size_t i = 0; i < n; i++) {
            buffer_pos++;
            buffer_pos = buffer_pos % delay_size;
            auto delay_sample = buffer[buffer_pos];
            buffer[buffer_pos].l = in[i].l + delay_sample.l * feedback;
            buffer[buffer_pos].r = in[i].r + delay_sample.r * feedback; 
            in[i].l = in[i].l * dry + delay_sample.l * wet;
            in[i].r = in[i].r * dry + delay_sample.r * wet;
        }
    }
};

}
}
