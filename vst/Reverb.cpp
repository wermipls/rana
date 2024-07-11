#include "common.hpp"

using namespace rana::audio;

struct EffectImpl : public EffectWrapper {
    Reverb fx;

    EffectImpl()
    {
        params.push_back({"wet", 0.25});
        params.push_back({"dry", 0.5});
        params.push_back({"width", 1.0});
        params.push_back({"roomsize", 0.5});
        params.push_back({"damp", 0.5});
        params.push_back({"lowpass", 1.0});
        params.push_back({"highpass", 0.0});
        fx = Reverb();
    }

    void vstSetParam(int index, float value)
    {
        params[index].value = value;
        fx.setParam(index, value);
    }

    std::vector<SampleStereo> process(std::vector<SampleStereo> in)
    {
        fx.process(in.data(), in.size());
        return in;
    }
};

EffectWrapper *newEffect()
{
    return new EffectImpl(); 
}

int getParamCount()
{
    return 7;
}
