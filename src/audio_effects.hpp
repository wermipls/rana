#include "audio_common.hpp"
#include <vector>

namespace rana {
namespace audio {

class Effect {
public:
    virtual std::vector<SampleStereo> process(std::vector<SampleStereo> in) = 0;
};

class Lowpass : public Effect {
    Hz sr;
    float coeff = 0;
    SampleStereo q{};

public:
    Lowpass(Hz sample_rate = 44100, Hz freq = 3000)
    {
        sr = sample_rate;
        setCutoff(freq);
    }

    std::vector<SampleStereo> process(std::vector<SampleStereo> in)
    {
        std::vector<SampleStereo> out(in.size());

        for (size_t i = 0; i < in.size(); i++) {
            q.l += (in[i].l - q.l) * coeff;
            q.r += (in[i].r - q.r) * coeff;
            out[i] = q;
        }

        return out;
    }

    void setCutoff(Hz freq)
    {
        coeff = factor_1pole(freq, sr);
    }
};

class Highpass : public Effect {
    Hz sr;
    float coeff = 0;
    SampleStereo q{};

public:
    Highpass(Hz sample_rate = 44100, Hz freq = 3000)
    {
        sr = sample_rate;
        setCutoff(freq);
    }

    std::vector<SampleStereo> process(std::vector<SampleStereo> in)
    {
        std::vector<SampleStereo> out(in.size());

        for (size_t i = 0; i < in.size(); i++) {
            q.l += (in[i].l - q.l) * coeff;
            q.r += (in[i].r - q.r) * coeff;
            out[i].l = in[i].l - q.l;
            out[i].r = in[i].r - q.r;
        }

        return out;
    }

    void setCutoff(Hz freq)
    {
        coeff = factor_1pole(freq, sr);
    }
};

}
}
