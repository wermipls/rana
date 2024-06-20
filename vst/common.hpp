#pragma once

#include "audioeffectx.h"
#include "crc.hpp"
#include <stdio.h>
#include <vector>
#include <string>
#include "../src/audio_effects.hpp"

#ifndef PLUGIN_NAME
    #define PLUGIN_NAME "default"
#endif
#ifndef PLUGIN_VENDOR
    #define PLUGIN_VENDOR "default"
#endif

int clamp(int a, int min, int max)
{
    if (a < min) return min;
    if (a > max) return max;
    return a;
}

float int2float(int a, int min, int max)
{
    a = clamp(a, min, max);
    a = a - min;
    float div = max - min;
    return (float)a / div;
}

int float2int(float a, int min, int max)
{
    float mul = max - min;
    a *= mul;
    a += min;
    return clamp(a, min, max);
}

float float2norm(int a, float min, float max)
{
    a = clamp(a, min, max);
    a = a - min;
    float div = max - min;
    return (float)a / div;
}

int norm2float(float a, float min, float max)
{
    float mul = max - min;
    a *= mul;
    a += min;
    return a;
}

struct Param {
    const char *name;
    const char *label;
    char valstr[kVstMaxParamStrLen];
    const float min, max;
    float value;
};

struct EffectWrapper {
    std::vector<Param> params;

    virtual void vstSetSamplingRate(float sr) = 0;
    virtual std::vector<rana::audio::SampleStereo> process(std::vector<rana::audio::SampleStereo> in) = 0;
    virtual void vstSetParam(int index, float value) = 0;
    virtual ~EffectWrapper() = default;
};

class VstEffect : public AudioEffectX
{
  public:
    VstEffect(audioMasterCallback audioMaster);
    ~VstEffect();

    // Processing
    virtual void processReplacing(float **in, float **out, VstInt32 frames);

    // Program
    virtual void setProgramName(char *name);
    virtual void getProgramName(char *name);

    // Parameters
    virtual void setParameter(VstInt32 index, float value);
    virtual float getParameter(VstInt32 index);
    virtual void getParameterLabel(VstInt32 index, char *label);
    virtual void getParameterDisplay(VstInt32 index, char *text);
    virtual void getParameterName(VstInt32 index, char *text);

    virtual bool getEffectName(char *name);
    virtual bool getVendorString(char *text);
    virtual bool getProductString(char *text);
    virtual VstInt32 getVendorVersion();

  protected:
    float sr = 44100;
    EffectWrapper *fx;
    char programName[kVstMaxProgNameLen + 1];
};

bool VstEffect::getEffectName(char *name) 
{
    vst_strncpy(name, PLUGIN_NAME, kVstMaxEffectNameLen);
    return true;
}

bool VstEffect::getProductString(char *text) 
{
    vst_strncpy(text, PLUGIN_NAME, kVstMaxProductStrLen);
    return true;
}

bool VstEffect::getVendorString(char *text) 
{
    vst_strncpy(text, PLUGIN_VENDOR, kVstMaxVendorStrLen);
    return true;
}

VstInt32 VstEffect::getVendorVersion() { return 1000; }

void VstEffect::setParameter(VstInt32 index, float value)
{
    if (index >= fx->params.size()) return;
    fx->vstSetParam(index, value);
}

float VstEffect::getParameter(VstInt32 index)
{
    if (index >= fx->params.size()) return 0;
    auto &p = fx->params[index];
    return float2norm(p.value, p.min, p.max);
}

void VstEffect::getParameterName(VstInt32 index, char *label) 
{
    if (index >= fx->params.size()) return;
    auto &p = fx->params[index];
    vst_strncpy(label, p.name, kVstMaxParamStrLen);
}

void VstEffect::getParameterDisplay(VstInt32 index, char *text) 
{
    if (index >= fx->params.size()) return;
    auto &p = fx->params[index];
    snprintf(text, kVstMaxParamStrLen, "%f", p.value);
}

void VstEffect::getParameterLabel(VstInt32 index, char *label) 
{
    if (index >= fx->params.size()) return;
    auto &p = fx->params[index];
    vst_strncpy(label, p.label, kVstMaxParamStrLen);
}

AudioEffect *createEffectInstance(audioMasterCallback audioMaster) 
{
    return new VstEffect(audioMaster);
}

EffectWrapper *newEffect();

VstEffect::VstEffect(audioMasterCallback audioMaster) : AudioEffectX(audioMaster, 1, 1)
{
    const char name[] = PLUGIN_NAME;
    setNumInputs(2);         // stereo in
    setNumOutputs(2);        // stereo out
    setUniqueID(crc32::crc32::calculate(name, sizeof(name))); // identify
    canProcessReplacing();   // supports replacing output

    fx = newEffect();

    vst_strncpy(programName, "Default", kVstMaxProgNameLen);
}

VstEffect::~VstEffect() 
{
    delete fx;
}

void VstEffect::setProgramName(char *name) 
{
    vst_strncpy(programName, name, kVstMaxProgNameLen);
}

void VstEffect::getProgramName(char *name) 
{
    vst_strncpy(name, programName, kVstMaxProgNameLen);
}

void VstEffect::processReplacing(float **in, float **out, VstInt32 frames) 
{

    std::vector<rana::audio::SampleStereo> buf(frames);

    for (size_t i = 0; i < frames; i++) {
        buf[i].l = in[0][i];
        buf[i].r = in[1][i];
    }

    buf = fx->process(buf);

    for (size_t i = 0; i < frames; i++) {
        out[0][i] = buf[i].l;
        out[1][i] = buf[i].r;
    }
}
