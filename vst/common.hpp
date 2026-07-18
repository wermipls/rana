#pragma once

#include "audioeffectx.h"
#include "crc.hpp"
#include <stdio.h>
#include <vector>
#include <string>
#define RANA_SUPERFLUOUS_VST_PARAMS
#include "../src/audio/effects.hpp"

#ifndef PLUGIN_VENDOR
    #define PLUGIN_VENDOR "default"
#endif

template <class RanaEffect>
class VstEffect : public AudioEffectX
{
    inline static int getParamCount() {
        RanaEffect fx;
        return fx.getParamCount();
    }

  public:
    VstEffect(audioMasterCallback audioMaster) : AudioEffectX(audioMaster, 1, getParamCount()) {
        auto effect_name = std::string("rana") + fx.getName();

        setNumInputs(2);         // stereo in
        setNumOutputs(2);        // stereo out
        setUniqueID(crc32::crc32::calculate(effect_name.c_str(), effect_name.length())); // identify
        canProcessReplacing();   // supports replacing output

        vst_strncpy(programName, "Default", kVstMaxProgNameLen);
    }
    ~VstEffect() {};

    // Processing
    virtual void processReplacing(float **in, float **out, VstInt32 frames)
    {
        // fixme: it would be best to not have an allocation here and instead
        // work on a fixed size buffer in chunks, but in practice it doesn't seem to matter.
        std::vector<rana::audio::SampleStereo> buf(frames);

        for (int i = 0; i < frames; i++) {
            buf[i].l = in[0][i];
            buf[i].r = in[1][i];
        }

        fx.process(buf.data(), buf.size());

        for (int i = 0; i < frames; i++) {
            out[0][i] = buf[i].l;
            out[1][i] = buf[i].r;
        }
    }

    // Program
    virtual void setProgramName(char *name) {
        vst_strncpy(programName, name, kVstMaxProgNameLen);
    }
    virtual void getProgramName(char *name) {
        vst_strncpy(name, programName, kVstMaxProgNameLen);
    }

    // Parameters
    virtual void setParameter(VstInt32 index, float value) {
        if (index >= fx.getParamCount()) return;
        fx.setParam(index, value);
    }
    virtual float getParameter(VstInt32 index) {
        if (index >= fx.getParamCount()) return 0;
        return fx.getParam(index);
    }

    // fixme: implement getParameterProperties.
    // the 8 character long names are really abysmal.
    virtual void getParameterName(VstInt32 index, char *text)
    {
        if (index >= fx.getParamCount()) return;
        vst_strncpy(text, fx.getParamName(index), kVstMaxParamStrLen);
    }

    virtual void getParameterLabel(VstInt32 index, char *label) {
        if (index >= fx.getParamCount()) return;
        fx.getParamLabel(index, label);
    }

    virtual void getParameterDisplay(VstInt32 index, char *text) {
        fx.getParamFmt(index, text);
    }

    virtual bool getEffectName(char *name) {
        vst_strncpy(name, effect_name.c_str(), kVstMaxEffectNameLen);
        return true;
    }

    virtual bool getProductString(char *name) {
        vst_strncpy(name, effect_name.c_str(), kVstMaxProductStrLen);
        return true;
    }

    virtual bool getVendorString(char *text) {
        vst_strncpy(text, PLUGIN_VENDOR, kVstMaxVendorStrLen);
        return true;
    }

    virtual VstInt32 getVendorVersion() { return 1000; }

  protected:
    std::string effect_name;
    float sr = 44100;
    RanaEffect fx;
    char programName[kVstMaxProgNameLen + 1];
};

// all this template bs just to finish it off with a macro, heh.
#define RANA_VST_EFFECT(type) \
    AudioEffect *createEffectInstance(audioMasterCallback audioMaster) \
    {                                                                  \
        return new VstEffect<type>(audioMaster);                       \
    }
