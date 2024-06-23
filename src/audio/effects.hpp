#pragma once

#include "common.hpp"
#include "freeverb/freeverb.h"
#include <vector>

namespace rana {
namespace audio {

using std::min, std::max, std::pow, std::fmod, std::abs, std::copysign;

class Effect {
public:
    virtual void process(SampleStereo *in, size_t n) = 0;
    virtual void setParam(int index, float value) = 0;
    virtual ~Effect() = default;
};

class Filter1Pole : public Effect {
    Hz sr;
    float coeff = 0;
    bool highpass;
    SampleStereo q{};

public:
    Filter1Pole(Hz sample_rate = 44100, bool is_highpass = false)
    {
        sr = sample_rate;
        highpass = is_highpass;
        setCutoff(3000);
    }

    void setCutoff(Hz freq)
    {
        coeff = factor_1pole(freq, sr);
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: setCutoff(pow(value, 2) * 19980 + 20); break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        if (highpass) {
            for (size_t i = 0; i < n; i++) {
                q.l += (in[i].l - q.l) * coeff;
                q.r += (in[i].r - q.r) * coeff;
                in[i].l -= q.l;
                in[i].r -= q.r;
            }
        } else {
            for (size_t i = 0; i < n; i++) {
                q.l += (in[i].l - q.l) * coeff;
                q.r += (in[i].r - q.r) * coeff;
                in[i] = q;
            }
        }
    }
};

class Reverb : public Effect {
    fv_Context ctx;

public:
    Reverb()
    {
        fv_init(&ctx);
        fv_set_samplerate(&ctx, 44100);
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: fv_set_wet(&ctx, value); break;
            case 1: fv_set_dry(&ctx, value); break;
            case 2: fv_set_width(&ctx, value); break;
            case 3: fv_set_roomsize(&ctx, value); break;
            case 4: fv_set_damp(&ctx, value); break;
            case 5: fv_set_lowpass(&ctx, value); break;
            case 6: fv_set_highpass(&ctx, value); break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        fv_process(&ctx, &in->l, n*2);
    }
};

class Delay : public Effect {
    static constexpr auto max_delay_seconds = 5.0;
    std::vector<SampleStereo> buffer;
    size_t buffer_pos = 0;
    size_t delay_size = 0;
    float feedback = 0.25;
    float wet = 0.25;
    float dry = 1.0;

public:
    Delay(Hz sample_rate = 44100)
    {
        buffer.resize(sample_rate * max_delay_seconds);
        for (auto &n : buffer) {
            n.l = 0;
            n.r = 0;
        }

        setDelay(0.2);
    }

    void setDelay(float value)
    {
        if (value > 1.0) value = 1.0;
        if (value < 0.0) value = 0.0;

        delay_size = buffer.size() * value;
        if (delay_size == 0) delay_size = 1;
        if (delay_size > buffer.size()) delay_size = buffer.size();

        buffer_pos = buffer_pos % delay_size;
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: wet = value; break;
            case 1: dry = value; break;
            case 2: feedback = value; break;
            case 3: setDelay(value); break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        for (size_t i = 0; i < n; i++) {
            buffer_pos++;
            buffer_pos = buffer_pos % delay_size;
            auto delay_sample = buffer[buffer_pos];
            buffer[buffer_pos].l = in[i].l + delay_sample.l * feedback;
            buffer[buffer_pos].r = in[i].r + delay_sample.r * feedback; 
            in[i].l = in[i].l * dry + delay_sample.l * wet;
            in[i].r = in[i].r * dry + delay_sample.r * wet;
        }
    }
};

class Distortion : public Effect {
    static constexpr float gain_multi = 127;
    enum Mode {
        Softclip,
        Shape,
        Fold,
        BadFold,
    } mode;
    float gain = 1;
    float dry = 0;
    float wet = 1;

public:
    Distortion()
    {
        mode = Fold;
    }

    void setAmount(float value)
    {
        gain = pow(value, 3) * gain_multi + 1;
    }

    void setMix(float value)
    {
        dry = 1.0f - value;
        wet = value;
    }

    void setMode(float value)
    {
        int i = value * 3;
        mode = (Mode)i;
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: setAmount(value); break;
            case 1: setMix(value); break;
            case 2: setMode(value); break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        switch (mode) 
        {
        case Softclip:
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                in[i].l = old.l * dry + max(min(in[i].l * gain, 1.0f), -1.0f) * wet;
                in[i].r = old.r * dry + max(min(in[i].r * gain, 1.0f), -1.0f) * wet;
            }
            break;
        case Shape:
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                auto ls = copysign(1.0f, in[i].l);
                auto rs = copysign(1.0f, in[i].r);
                in[i].l = old.l * dry + max(min(pow(abs(in[i].l), 1.0f / gain), 1.0f), -1.0f) * ls * wet;
                in[i].r = old.r * dry + max(min(pow(abs(in[i].r), 1.0f / gain), 1.0f), -1.0f) * rs * wet;
            }
            break;
        case Fold:
            for (size_t i = 0; i < n; i++) {
                auto ls = copysign(1.0f, in[i].l);
                auto rs = copysign(1.0f, in[i].r);
                auto l = abs(in[i].l * gain);
                auto r = abs(in[i].r * gain);
                auto lf = fmod(l + 1.0f, 4.0f);
                auto rf = fmod(r + 1.0f, 4.0f);
                l = fmod(l + 1.0f, 2.0f);
                r = fmod(r + 1.0f, 2.0f);
                if (lf >= 2.0f) l = 2.0f - l;
                if (rf >= 2.0f) r = 2.0f - r;
                in[i].l = in[i].l * dry + (l - 1.0) * ls * wet;
                in[i].r = in[i].r * dry + (r - 1.0) * rs * wet;
            }
            break;
        case BadFold:
            for (size_t i = 0; i < n; i++) {
                auto l = in[i].l * gain;
                auto r = in[i].r * gain;
                l = fmod(l, 1.0f);
                r = fmod(r, 1.0f);
                auto lf = abs(fmod(in[i].l, 2.0f));
                auto rf = abs(fmod(in[i].r, 2.0f));
                lf = (lf > 1.0f) ? -1.0f : 1.0f;
                rf = (rf > 1.0f) ? -1.0f : 1.0f;
                in[i].l = in[i].l * dry + l * lf * wet;
                in[i].r = in[i].r * dry + r * rf * wet;
            }
            break;
        }
    }
};

class Bitcrush : public Effect {
    int bits = 16;
    float sr, rate;
    float t = 0;
    SampleStereo prev = {0,0};

    inline static int bitcrush(int a, int bits)
    {
        int shift = (16 - bits);
        a = a >> shift;
        if (a < 0) a++; // compensate for two's complement
        return a << shift;
    }

    inline static int clamp(int a, int min, int max)
    {
        if (a < min) return min;
        if (a > max) return max;
        return a;
    }

    inline static int float2int(float a, int min, int max)
    {
        float mul = max - min;
        a *= mul;
        a += min;
        return clamp(a, min, max);
    }

public:
    Bitcrush(float sample_rate = 44100) : sr{sample_rate}
    {
        setRate(44100);
    }

    void setBits(float value)
    {
        bits = float2int(value, 2, 16);
    }

    void setRate(float value)
    {
        rate = 44100.0f * pow(value, 2);
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: setBits(value); break;
            case 1: setRate(value); break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        for (size_t i = 0; i < n; i++) {
            // rate
            t += rate / sr;
            if (t >= 1.0f) {
                t -= 1;
                prev = in[i];
            }

            // bitcrush
            int l = prev.l * 32768;
            int r = prev.r * 32768;

            l = bitcrush(l, bits);
            r = bitcrush(r, bits);

            in[i].l = l / 32768.f;
            in[i].r = r / 32768.f;
        }
    }
};

class Compressor : public Effect {
    static constexpr float PeakSmoothingHz = 20.0f;
    float sr;
    float pp_coeff;
    float min = 0;
    float max = 0;
    float pp = 0;
    float attack_coeff;
    float release_coeff;
    float threshold_db = 0.8;
    float volume_actual = 1.0;
    float volume_target = 0;
    float makeup = 1;
    float ratio = 0.5;

    void peakToPeak(SampleStereo value)
    {
        max *= pp_coeff;
        max = std::max(max,  value.l);
        max = std::max(max,  value.r);
        max = std::max(max, -value.l);
        max = std::max(max, -value.r);
        pp = dB(std::abs(max));
    }

    void processVolume()
    {
        if (volume_actual > volume_target) {
            volume_actual += (volume_target - volume_actual) * attack_coeff;
        } else {
            volume_actual += (volume_target - volume_actual) * release_coeff;
        }
    }

    static inline float dB(float volume)
    {
        return 20 * std::log10(volume);
    }

    static inline float from_dB(float a)
    {
        return std::pow(10, a/20);
    }

public:
    Compressor(float sample_rate = 44100) : sr{sample_rate}
    {
        pp_coeff = 1.0f - factor_1pole(PeakSmoothingHz, sr);
    }

    virtual void setParam(int index, float value)
    {
        switch (index) {
            case 0: threshold_db = dB(std::pow(value, 3) * 0.999f + 0.001f); break;
            case 1: attack_coeff  = factor_1pole(1.0f + std::pow(1.0f - value, 10) * 22049.0f, sr); break;
            case 2: release_coeff = factor_1pole(0.1f + std::pow(1.0f - value, 10) * 999.9f, sr); break;
            case 3: ratio = value; break; 
            case 4: makeup = std::pow(value, 3) * 16.0f + 1.0f; break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        for (size_t i = 0; i < n; i++) {
            peakToPeak(in[i]);
            auto delta_db = pp - threshold_db;
            auto target_db = 0;
            if (delta_db > 0.0f) target_db -= delta_db * ratio;
            volume_target = from_dB(target_db);
            volume_target *= makeup;
            processVolume();
            in[i].l *= volume_actual;
            in[i].r *= volume_actual;
        }
    }
};

// adapted from airwindows' Galactic effect plugin
// airwindows uses the MIT license
// https://github.com/airwindows/airwindows
class Galactic : public Effect {
    float sr;

    float iirAL;
    float iirBL;

    float aIL[6480];
    float aJL[3660];
    float aKL[1720];
    float aLL[680];

    float aAL[9700];
    float aBL[6000];
    float aCL[2320];
    float aDL[940];

    float aEL[15220];
    float aFL[8460];
    float aGL[4540];
    float aHL[3200];

    float aML[3111];
    float aMR[3111];
    float vibML, vibMR, depthM, oldfpd;

    float feedbackAL;
    float feedbackBL;
    float feedbackCL;
    float feedbackDL;

    float lastRefL[7];
    float thunderL;

    float iirAR;
    float iirBR;

    float aIR[6480];
    float aJR[3660];
    float aKR[1720];
    float aLR[680];

    float aAR[9700];
    float aBR[6000];
    float aCR[2320];
    float aDR[940];

    float aER[15220];
    float aFR[8460];
    float aGR[4540];
    float aHR[3200];

    float feedbackAR;
    float feedbackBR;
    float feedbackCR;
    float feedbackDR;

    float lastRefR[7];
    float thunderR;

    int countA, delayA;
    int countB, delayB;
    int countC, delayC;
    int countD, delayD;
    int countE, delayE;
    int countF, delayF;
    int countG, delayG;
    int countH, delayH;
    int countI, delayI;
    int countJ, delayJ;
    int countK, delayK;
    int countL, delayL;
    int countM, delayM;
    int cycle; // all these ints are shared across channels, not duplicated

    float vibM;

    uint32_t fpdL;
    uint32_t fpdR;

    // internal constants, calculated every process call in original,
    // but i'd rather only recalculate them when necessary
    float regen, attenuate, lowpass, drift, size, wet;
    int cycleEnd;

    static constexpr auto A = 0;
    static constexpr auto B = 1;
    static constexpr auto C = 2;
    static constexpr auto D = 3;
    static constexpr auto E = 4;
    float param[5];

    void update()
    {
        float overallscale = 1.0f / 44100.0f * sr;

        cycleEnd = floor(overallscale);
        if (cycleEnd < 1) cycleEnd = 1;
        if (cycleEnd > 4) cycleEnd = 4;
        // this is going to be 2 for 88.1 or 96k, 3 for silly people, 4 for 176 or 192k
        if (cycle > cycleEnd-1) cycle = cycleEnd - 1; // sanity check

        regen = 0.0625f + ((1.0f - param[A]) * 0.0625f);
        attenuate = (1.0f - (regen / 0.125f)) * 1.333f;
        lowpass = pow(1.00001f - (1.0f - param[B]), 2.0f) / sqrt(overallscale);
        drift = pow(param[C], 3) * 0.001f;
        size = (param[D] * 1.77f) + 0.1f;
        wet = 1.0f - pow(1.0f - param[E], 3);

        delayI = 3407.0f * size;
        delayJ = 1823.0f * size;
        delayK = 859.0f  * size;
        delayL = 331.0f  * size;
        delayA = 4801.0f * size;
        delayB = 2909.0f * size;
        delayC = 1153.0f * size;
        delayD = 461.0f  * size;
        delayE = 7607.0f * size;
        delayF = 4217.0f * size;
        delayG = 2269.0f * size;
        delayH = 1597.0f * size;
        delayM = 256;
    }

public:
    Galactic(float sample_rate = 44100) : sr{sample_rate}
    {
        param[A] = 0.5;
        param[B] = 0.5;
        param[C] = 0.5;
        param[D] = 1.0;
        param[E] = 1.0;

        iirAL = 0.0;
        iirAR = 0.0;
        iirBL = 0.0;
        iirBR = 0.0;

        for (int count = 0; count < 6479; count++) {
            aIL[count] = 0.0;
            aIR[count] = 0.0;
        }
        for (int count = 0; count < 3659; count++) {
            aJL[count] = 0.0;
            aJR[count] = 0.0;
        }
        for (int count = 0; count < 1719; count++) {
            aKL[count] = 0.0;
            aKR[count] = 0.0;
        }
        for (int count = 0; count < 679; count++) {
            aLL[count] = 0.0;
            aLR[count] = 0.0;
        }

        for (int count = 0; count < 9699; count++) {
            aAL[count] = 0.0;
            aAR[count] = 0.0;
        }
        for (int count = 0; count < 5999; count++) {
            aBL[count] = 0.0;
            aBR[count] = 0.0;
        }
        for (int count = 0; count < 2319; count++) {
            aCL[count] = 0.0;
            aCR[count] = 0.0;
        }
        for (int count = 0; count < 939; count++) {
            aDL[count] = 0.0;
            aDR[count] = 0.0;
        }

        for (int count = 0; count < 15219; count++) {
            aEL[count] = 0.0;
            aER[count] = 0.0;
        }
        for (int count = 0; count < 8459; count++) {
            aFL[count] = 0.0;
            aFR[count] = 0.0;
        }
        for (int count = 0; count < 4539; count++) {
            aGL[count] = 0.0;
            aGR[count] = 0.0;
        }
        for (int count = 0; count < 3199; count++) {
            aHL[count] = 0.0;
            aHR[count] = 0.0;
        }

        for (int count = 0; count < 3110; count++) {
            aML[count] = aMR[count] = 0.0;
        }

        feedbackAL = 0.0;
        feedbackAR = 0.0;
        feedbackBL = 0.0;
        feedbackBR = 0.0;
        feedbackCL = 0.0;
        feedbackCR = 0.0;
        feedbackDL = 0.0;
        feedbackDR = 0.0;

        for (int count = 0; count < 6; count++) {
            lastRefL[count] = 0.0;
            lastRefR[count] = 0.0;
        }

        thunderL = 0;
        thunderR = 0;

        countI = 1;
        countJ = 1;
        countK = 1;
        countL = 1;

        countA = 1;
        countB = 1;
        countC = 1;
        countD = 1;

        countE = 1;
        countF = 1;
        countG = 1;
        countH = 1;
        countM = 1;
        // the predelay
        cycle = 0;

        vibM = 3.0;

        oldfpd = 429496.7295;

        fpdL = 1.0;
        while (fpdL < 16386) fpdL = rand() * UINT32_MAX;
        fpdR = 1.0;
        while (fpdR < 16386) fpdR = rand() * UINT32_MAX;

        update();
    }

    virtual void setParam(int index, float value)
    {
        if (index >= 0 && index < 5) {
            param[index] = value;
        }
        update();
    }

    virtual void process(SampleStereo *in, size_t n_samples)
    {
        float *in1 = &in->l;
        float *in2 = &in->r;
        float *out1 = in1;
        float *out2 = in2;

        for (size_t i = 0; i < n_samples; i++) {
            auto inputSampleL = *in1;
            auto inputSampleR = *in2;
            if (fabs(inputSampleL) < 1.18e-23f) inputSampleL = fpdL * 1.18e-17f;
            if (fabs(inputSampleR) < 1.18e-23f) inputSampleR = fpdR * 1.18e-17f;
            auto drySampleL = inputSampleL;
            auto drySampleR = inputSampleR;

            vibM += (oldfpd * drift);
            if (vibM > (M_PI * 2.0f)) {
                vibM = 0.0f;
                oldfpd = 0.4294967295f + (fpdL * 0.0000000000618f);
            }

            aML[countM] = inputSampleL * attenuate;
            aMR[countM] = inputSampleR * attenuate;
            countM++;
            if (countM < 0 || countM > delayM) countM = 0;

            float offsetML = (sin(vibM) + 1.0f) * 127.0f;
            float offsetMR = (sin(vibM + (M_PI / 2.0f)) + 1.0) * 127.0f;
            int workingML = countM + offsetML;
            int workingMR = countM + offsetMR;
            float interpolML = (aML[workingML - ((workingML > delayM) ? delayM + 1 : 0)]
                             * (1 - (offsetML - floor(offsetML))));
            interpolML += (aML[workingML + 1 - ((workingML + 1 > delayM) ? delayM + 1 : 0)]
                        * ((offsetML - floor(offsetML))));
            float interpolMR = (aMR[workingMR - ((workingMR > delayM) ? delayM + 1 : 0)]
                             * (1 - (offsetMR - floor(offsetMR))));
            interpolMR +=
                (aMR[workingMR + 1 - ((workingMR + 1 > delayM) ? delayM + 1 : 0)] *
                 ((offsetMR - floor(offsetMR))));
            inputSampleL = interpolML;
            inputSampleR = interpolMR;
            // predelay that applies vibrato
            // want vibrato speed AND depth like in MatrixVerb

            iirAL = (iirAL * (1.0 - lowpass)) + (inputSampleL * lowpass);
            inputSampleL = iirAL;
            iirAR = (iirAR * (1.0 - lowpass)) + (inputSampleR * lowpass);
            inputSampleR = iirAR;
            // initial filter

            cycle++;
            if (cycle == cycleEnd) { // hit the end point and we do a reverb sample
                aIL[countI] = inputSampleL + (feedbackAR * regen);
                aJL[countJ] = inputSampleL + (feedbackBR * regen);
                aKL[countK] = inputSampleL + (feedbackCR * regen);
                aLL[countL] = inputSampleL + (feedbackDR * regen);
                aIR[countI] = inputSampleR + (feedbackAL * regen);
                aJR[countJ] = inputSampleR + (feedbackBL * regen);
                aKR[countK] = inputSampleR + (feedbackCL * regen);
                aLR[countL] = inputSampleR + (feedbackDL * regen);

                countI++;
                if (countI < 0 || countI > delayI) countI = 0;
                countJ++;
                if (countJ < 0 || countJ > delayJ) countJ = 0;
                countK++;
                if (countK < 0 || countK > delayK) countK = 0;
                countL++;
                if (countL < 0 || countL > delayL) countL = 0;

                float outIL = aIL[countI - ((countI > delayI) ? delayI + 1 : 0)];
                float outJL = aJL[countJ - ((countJ > delayJ) ? delayJ + 1 : 0)];
                float outKL = aKL[countK - ((countK > delayK) ? delayK + 1 : 0)];
                float outLL = aLL[countL - ((countL > delayL) ? delayL + 1 : 0)];
                float outIR = aIR[countI - ((countI > delayI) ? delayI + 1 : 0)];
                float outJR = aJR[countJ - ((countJ > delayJ) ? delayJ + 1 : 0)];
                float outKR = aKR[countK - ((countK > delayK) ? delayK + 1 : 0)];
                float outLR = aLR[countL - ((countL > delayL) ? delayL + 1 : 0)];
                // first block: now we have four outputs

                aAL[countA] = (outIL - (outJL + outKL + outLL));
                aBL[countB] = (outJL - (outIL + outKL + outLL));
                aCL[countC] = (outKL - (outIL + outJL + outLL));
                aDL[countD] = (outLL - (outIL + outJL + outKL));
                aAR[countA] = (outIR - (outJR + outKR + outLR));
                aBR[countB] = (outJR - (outIR + outKR + outLR));
                aCR[countC] = (outKR - (outIR + outJR + outLR));
                aDR[countD] = (outLR - (outIR + outJR + outKR));

                countA++;
                if (countA < 0 || countA > delayA) countA = 0;
                countB++;
                if (countB < 0 || countB > delayB) countB = 0;
                countC++;
                if (countC < 0 || countC > delayC) countC = 0;
                countD++;
                if (countD < 0 || countD > delayD) countD = 0;

                float outAL = aAL[countA - ((countA > delayA) ? delayA + 1 : 0)];
                float outBL = aBL[countB - ((countB > delayB) ? delayB + 1 : 0)];
                float outCL = aCL[countC - ((countC > delayC) ? delayC + 1 : 0)];
                float outDL = aDL[countD - ((countD > delayD) ? delayD + 1 : 0)];
                float outAR = aAR[countA - ((countA > delayA) ? delayA + 1 : 0)];
                float outBR = aBR[countB - ((countB > delayB) ? delayB + 1 : 0)];
                float outCR = aCR[countC - ((countC > delayC) ? delayC + 1 : 0)];
                float outDR = aDR[countD - ((countD > delayD) ? delayD + 1 : 0)];
                // second block: four more outputs

                aEL[countE] = (outAL - (outBL + outCL + outDL));
                aFL[countF] = (outBL - (outAL + outCL + outDL));
                aGL[countG] = (outCL - (outAL + outBL + outDL));
                aHL[countH] = (outDL - (outAL + outBL + outCL));
                aER[countE] = (outAR - (outBR + outCR + outDR));
                aFR[countF] = (outBR - (outAR + outCR + outDR));
                aGR[countG] = (outCR - (outAR + outBR + outDR));
                aHR[countH] = (outDR - (outAR + outBR + outCR));

                countE++;
                if (countE < 0 || countE > delayE) countE = 0;
                countF++;
                if (countF < 0 || countF > delayF) countF = 0;
                countG++;
                if (countG < 0 || countG > delayG) countG = 0;
                countH++;
                if (countH < 0 || countH > delayH) countH = 0;

                float outEL = aEL[countE - ((countE > delayE) ? delayE + 1 : 0)];
                float outFL = aFL[countF - ((countF > delayF) ? delayF + 1 : 0)];
                float outGL = aGL[countG - ((countG > delayG) ? delayG + 1 : 0)];
                float outHL = aHL[countH - ((countH > delayH) ? delayH + 1 : 0)];
                float outER = aER[countE - ((countE > delayE) ? delayE + 1 : 0)];
                float outFR = aFR[countF - ((countF > delayF) ? delayF + 1 : 0)];
                float outGR = aGR[countG - ((countG > delayG) ? delayG + 1 : 0)];
                float outHR = aHR[countH - ((countH > delayH) ? delayH + 1 : 0)];
                // third block: final outputs

                feedbackAL = (outEL - (outFL + outGL + outHL));
                feedbackBL = (outFL - (outEL + outGL + outHL));
                feedbackCL = (outGL - (outEL + outFL + outHL));
                feedbackDL = (outHL - (outEL + outFL + outGL));
                feedbackAR = (outER - (outFR + outGR + outHR));
                feedbackBR = (outFR - (outER + outGR + outHR));
                feedbackCR = (outGR - (outER + outFR + outHR));
                feedbackDR = (outHR - (outER + outFR + outGR));
                // which we need to feed back into the input again, a bit

                inputSampleL = (outEL + outFL + outGL + outHL) / 8.0f;
                inputSampleR = (outER + outFR + outGR + outHR) / 8.0f;
                // and take the final combined sum of outputs
                if (cycleEnd == 4) {
                    lastRefL[0] = lastRefL[4]; // start from previous last
                    lastRefL[2] = (lastRefL[0] + inputSampleL) / 2; // half
                    lastRefL[1] = (lastRefL[0] + lastRefL[2]) / 2;  // one quarter
                    lastRefL[3] = (lastRefL[2] + inputSampleL) / 2; // three quarters
                    lastRefL[4] = inputSampleL;                     // full
                    lastRefR[0] = lastRefR[4]; // start from previous last
                    lastRefR[2] = (lastRefR[0] + inputSampleR) / 2; // half
                    lastRefR[1] = (lastRefR[0] + lastRefR[2]) / 2;  // one quarter
                    lastRefR[3] = (lastRefR[2] + inputSampleR) / 2; // three quarters
                    lastRefR[4] = inputSampleR;                     // full
                }
                if (cycleEnd == 3) {
                    lastRefL[0] = lastRefL[3]; // start from previous last
                    lastRefL[2] = (lastRefL[0] + lastRefL[0] + inputSampleL) / 3; // third
                    lastRefL[1] = (lastRefL[0] + inputSampleL + inputSampleL) / 3; // two thirds
                    lastRefL[3] = inputSampleL; // full
                    lastRefR[0] = lastRefR[3]; // start from previous last
                    lastRefR[2] = (lastRefR[0] + lastRefR[0] + inputSampleR) / 3; // third
                    lastRefR[1] = (lastRefR[0] + inputSampleR + inputSampleR) / 3; // two thirds
                    lastRefR[3] = inputSampleR;                          // full
                }
                if (cycleEnd == 2) {
                    lastRefL[0] = lastRefL[2]; // start from previous last
                    lastRefL[1] = (lastRefL[0] + inputSampleL) / 2; // half
                    lastRefL[2] = inputSampleL;                     // full
                    lastRefR[0] = lastRefR[2]; // start from previous last
                    lastRefR[1] = (lastRefR[0] + inputSampleR) / 2; // half
                    lastRefR[2] = inputSampleR;                     // full
                }
                if (cycleEnd == 1) {
                    lastRefL[0] = inputSampleL;
                    lastRefR[0] = inputSampleR;
                }
                cycle = 0; // reset
                inputSampleL = lastRefL[cycle];
                inputSampleR = lastRefR[cycle];
            } else {
                inputSampleL = lastRefL[cycle];
                inputSampleR = lastRefR[cycle];
                // we are going through our references now
            }

            iirBL = (iirBL * (1.0f - lowpass)) + (inputSampleL * lowpass);
            inputSampleL = iirBL;
            iirBR = (iirBR * (1.0f - lowpass)) + (inputSampleR * lowpass);
            inputSampleR = iirBR;
            // end filter

            if (wet < 1.0f) {
                inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0f - wet));
                inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0f - wet));
            }

            /*
            //begin 32 bit stereo floating point dither
            int expon; frexpf((float)inputSampleL, &expon);
            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 5.5e-36l *
            pow(2,expon+62)); frexpf((float)inputSampleR, &expon); fpdR ^= fpdR << 13;
            fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5; inputSampleR +=
            ((double(fpdR)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
            //end 32 bit stereo floating point dither
            */

            *out1 = inputSampleL;
            *out2 = inputSampleR;

            in1 += 2;
            in2 += 2;
            out1 += 2;
            out2 += 2;
        }
    }
};

}
}
