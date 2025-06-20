#define RANA_SUPERFLUOUS_VST_PARAMS
#include "common.hpp"

using namespace rana::audio;

struct EffectImpl : public EffectWrapper {
    Filter1Pole fx;

    EffectImpl()
    {
        params.push_back({"cutoff", 0.5});
        fx = Filter1Pole(44100, true);
    }

    void vstSetParam(int index, float value)
    {
        params[index].value = value;
        fx.setParam(index, value);
    }

    bool vstParamFmt(int index, char *str)
    {
        fx.getParamFmt(index, str);
        return true;
    }

    bool vstParamLabel(int index, char *str)
    {
        fx.getParamLabel(index, str);
        return true;
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
    return 1;
}
