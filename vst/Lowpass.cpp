#include "common.hpp"

using namespace rana::audio;

struct EffectImpl : public EffectWrapper {
    Lowpass fx;

    EffectImpl()
    {
        params.push_back({"cutoff", "Hz", "", 1, 10000, 3000});
        fx = Lowpass(44100);
    }

    void vstSetSamplingRate(float sr)
    {
        fx = Lowpass(sr);
    }

    void vstSetParam(int index, float value)
    {
        auto adjusted = norm2float(value, params[index].min, params[index].max);
        params[index].value = adjusted;
        switch (index) {
            case 0: fx.setCutoff(adjusted); break;
        }
    }

    std::vector<SampleStereo> process(std::vector<SampleStereo> in)
    {
        return fx.process(in);
    }
};

EffectWrapper *newEffect()
{
    return new EffectImpl(); 
}
