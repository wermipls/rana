#include "audio_common.hpp"
#include "freeverb/freeverb.h"
#include <vector>

namespace rana {
namespace audio {

class Effect {
public:
    virtual void process(SampleStereo *in, size_t n) = 0;
    virtual void setParam(int index, float value) = 0;
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
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        fv_process(&ctx, &in->l, n*2);
    }
};

}
}
