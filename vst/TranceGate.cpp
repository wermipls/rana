#include "common.hpp"

using namespace rana::audio;

struct EffectImpl : public EffectWrapper {
    TranceGate fx;

    EffectImpl()
    {
        fx = TranceGate(44100);
        int n_params = fx.getParamCount();
        for (int i = 0; i < n_params; i++) {
            params.push_back({fx.getParamName(i), fx.getParam(i)});
        }
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
