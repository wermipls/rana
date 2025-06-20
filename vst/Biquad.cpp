#define RANA_SUPERFLUOUS_VST_PARAMS
#include "common.hpp"

using namespace rana::audio;

struct EffectImpl : public EffectWrapper {
    Biquad fx;

    EffectImpl()
    {
        params.push_back({"mode", 0.0});
        params.push_back({"cutoff", 0.5});
        params.push_back({"q", 0.2729});
        params.push_back({"gain", 0.5});
        fx = Biquad();
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
    return 4;
}
