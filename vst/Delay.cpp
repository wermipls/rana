#include "common.hpp"

using namespace rana::audio;

struct EffectImpl : public EffectWrapper {
    Delay fx;

    EffectImpl()
    {
        params.push_back({"wet", 0.25});
        params.push_back({"dry", 1.0});
        params.push_back({"feedback", 0.25});
        params.push_back({"delay", 0.1});
        fx = Delay(44100);
    }

    void vstSetSamplingRate(float sr)
    {
        fx = Delay(sr);
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
    return 4;
}
