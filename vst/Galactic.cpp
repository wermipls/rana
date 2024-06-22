#include "common.hpp"

using namespace rana::audio;

struct EffectImpl : public EffectWrapper {
    Galactic fx;

    EffectImpl()
    {
        params.push_back({"replace", 0.5});
        params.push_back({"brightness", 0.5});
        params.push_back({"detune", 0.5});
        params.push_back({"bigness", 1.0});
        params.push_back({"dry/wet", 1.0});
        fx = Galactic();
    }

    void vstSetSamplingRate(float sr)
    {
        fx = Galactic(sr);
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
    return 5;
}
