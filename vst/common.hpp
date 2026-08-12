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
        canDoubleReplacing();

        vst_strncpy(programName, "Default", kVstMaxProgNameLen);

        // A bunch of hosts allow param strings longer than 8.
        // Why? There's a bunch of plugins that are either buggy or simply don't care
        // (and rightfully so, because 8 characters + null is some kind of cruel joke),
        // so it's better to overallocate than deal with potential memory corruption.
        // Some DAWs explicitly consider it an extension, for instance Reaper
        // (see https://www.reaper.fm/sdk/vst/vst_ext.php).
        //
        // We err on the safe side and only override the length for DAWs with known behavior.
        char vendor[64+1];
        getHostVendorString(vendor);
        if (strcmp(vendor, "Renoise") == 0) {
            param_str_len = 64;
        } else if (strcmp(vendor, "Cockos") == 0) {
            param_str_len = 255;
        }
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

    virtual void processDoubleReplacing(double **in, double **out, VstInt32 frames)
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

    virtual void getParameterName(VstInt32 index, char *text)
    {
        if (index >= fx.getParamCount()) return;
        vst_strncpy(text, fx.getParamName(index), param_str_len);
    }

    virtual void getParameterLabel(VstInt32 index, char *label) {
        if (index >= fx.getParamCount()) return;

        constexpr bool has_getParamLabel = requires(RanaEffect &fx) {
            fx.getParamLabel(int(0), (char *)(nullptr));
        };

        if constexpr (has_getParamLabel) {
            fx.getParamLabel(index, label);
        } else {
            label[0] = 0;
        }
    }

    virtual void getParameterDisplay(VstInt32 index, char *text) {
        if (index >= fx.getParamCount()) return;

        constexpr bool has_getParamFmt = requires(RanaEffect &fx) {
            fx.getParamFmt(int(0), (char *)(nullptr));
        };

        if constexpr (has_getParamFmt) {
            fx.getParamFmt(index, text);
        } else {
            snprintf(text, param_str_len + 1, "%f", fx.getParam(index));
        }
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
    size_t param_str_len = 8;
};

// all this template bs just to finish it off with a macro, heh.
#define RANA_VST_EFFECT(type) \
    AudioEffect *createEffectInstance(audioMasterCallback audioMaster) \
    {                                                                  \
        return new VstEffect<type>(audioMaster);                       \
    }
