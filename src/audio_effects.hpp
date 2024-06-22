#include "audio_common.hpp"
#include "freeverb/freeverb.h"
#include <vector>

namespace rana {
namespace audio {

using std::min, std::max, std::pow, std::fmod, std::abs, std::copysign;

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
            case 0: setCutoff(pow(value, 2) * 19980 + 20); break;
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

class Distortion : public Effect {
    static constexpr float gain_multi = 127;
    enum Mode {
        Softclip,
        Shape,
        Fold,
        BadFold,
    } mode;
    float gain = 1;
    float dry = 0;
    float wet = 1;

public:
    Distortion()
    {
        mode = Fold;
    }

    void setAmount(float value)
    {
        gain = pow(value, 3) * gain_multi + 1;
    }

    void setMix(float value)
    {
        dry = 1.0f - value;
        wet = value;
    }

    void setMode(float value)
    {
        int i = value * 3;
        mode = (Mode)i;
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: setAmount(value); break;
            case 1: setMix(value); break;
            case 2: setMode(value); break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        switch (mode) 
        {
        case Softclip:
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                in[i].l = old.l * dry + max(min(in[i].l * gain, 1.0f), -1.0f) * wet;
                in[i].r = old.r * dry + max(min(in[i].r * gain, 1.0f), -1.0f) * wet;
            }
            break;
        case Shape:
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                auto ls = copysign(1.0f, in[i].l);
                auto rs = copysign(1.0f, in[i].r);
                in[i].l = old.l * dry + max(min(pow(abs(in[i].l), 1.0f / gain), 1.0f), -1.0f) * ls * wet;
                in[i].r = old.r * dry + max(min(pow(abs(in[i].r), 1.0f / gain), 1.0f), -1.0f) * rs * wet;
            }
            break;
        case Fold:
            for (size_t i = 0; i < n; i++) {
                auto ls = copysign(1.0f, in[i].l);
                auto rs = copysign(1.0f, in[i].r);
                auto l = abs(in[i].l * gain);
                auto r = abs(in[i].r * gain);
                auto lf = fmod(l + 1.0f, 4.0f);
                auto rf = fmod(r + 1.0f, 4.0f);
                l = fmod(l + 1.0f, 2.0f);
                r = fmod(r + 1.0f, 2.0f);
                if (lf >= 2.0f) l = 2.0f - l;
                if (rf >= 2.0f) r = 2.0f - r;
                in[i].l = in[i].l * dry + (l - 1.0) * ls * wet;
                in[i].r = in[i].r * dry + (r - 1.0) * rs * wet;
            }
            break;
        case BadFold:
            for (size_t i = 0; i < n; i++) {
                auto l = in[i].l * gain;
                auto r = in[i].r * gain;
                l = fmod(l, 1.0f);
                r = fmod(r, 1.0f);
                auto lf = abs(fmod(in[i].l, 2.0f));
                auto rf = abs(fmod(in[i].r, 2.0f));
                lf = (lf > 1.0f) ? -1.0f : 1.0f;
                rf = (rf > 1.0f) ? -1.0f : 1.0f;
                in[i].l = in[i].l * dry + l * lf * wet;
                in[i].r = in[i].r * dry + r * rf * wet;
            }
            break;
        }
    }
};

class Bitcrush : public Effect {
    int bits = 16;
    float sr, rate;
    float t = 0;
    SampleStereo prev = {0,0};

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
        setRate(44100);
    }

    void setBits(float value)
    {
        bits = float2int(value, 2, 16);
    }

    void setRate(float value)
    {
        rate = 44100.0f * pow(value, 2);
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: setBits(value); break;
            case 1: setRate(value); break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        for (size_t i = 0; i < n; i++) {
            // rate
            t += rate / sr;
            if (t >= 1.0f) {
                t -= 1;
                prev = in[i];
            }

            // bitcrush
            int l = prev.l * 32768;
            int r = prev.r * 32768;

            l = bitcrush(l, bits);
            r = bitcrush(r, bits);

            in[i].l = l / 32768.f;
            in[i].r = r / 32768.f;
        }
    }
};

}
}
