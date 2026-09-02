#pragma once

#include "common.hpp"
#include "serializer.hpp"

namespace rana {
namespace audio {

// Effect interface.
// Roughly similar to VST API, but much simplifed.
//
// Some notes:
// - The only methods that need to be implemented are `process()` and `getName()`.
// - If a constructor has arguments, the first one should be the sample rate
//   and the rest (if any) optional, as the VST wrapper machinery depends on it.
// - The user will call `setParam()` for each parameter before calling `process()`.
//   Despite that, `getParam()` must return sane default values even before that.
// - No thread safety is assumed. Any method calls will happen synchronously,
//   though not necessarily on the same thread.
class Effect {
public:
    virtual void process(SampleStereo *in, size_t n) = 0;
    virtual const char *getName() = 0;

    virtual void setParam(int index, float value) { (void)index; (void)value; }
    virtual float getParam(int index) { (void)index; return 0; }
    virtual int getParamCount() { return 0; }
    virtual const char *getParamName(int index) { (void)index; return "n/a"; }

    // Not used by anything at the moment.
    virtual void setBPM(float bpm, bool retrigger = true) { (void)bpm; (void)retrigger; }
    virtual bool isSerializable() { return false; }
    virtual void serialize(Serializer &s) { (void)s; }

    virtual ~Effect() = default;
};

}
}
