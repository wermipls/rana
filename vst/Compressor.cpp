#include "common.hpp"

using namespace rana::audio;

struct EffectImpl : public EffectWrapper {
    Compressor fx;

    EffectImpl()
    {
        params.push_back({"threshold", 0.8});
        params.push_back({"attack", 0.2});
        params.push_back({"release", 0.5});
        params.push_back({"ratio", 0.5});
        params.push_back({"makeup", 0.0});
        fx = Compressor(44100);
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
