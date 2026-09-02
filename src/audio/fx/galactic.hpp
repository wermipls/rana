#pragma once

#include "audio/effect.hpp"
#include <cmath>
#include <tracy/Tracy.hpp>
#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    #include <stdio.h>
#endif

namespace rana::audio {

using std::abs, std::floor, std::pow;

// adapted from airwindows' Galactic effect plugin
// airwindows uses the MIT license
// https://github.com/airwindows/airwindows
class Galactic : public Effect {
    static constexpr auto paramCount = 5;
    const char *paramNames[paramCount] = {
        "replace",
        "brightness",
        "detune",
        "bigness",
        "dry/wet",
    };

    double sr;

    SampleStereo iirA;
    SampleStereo iirB;

    SampleStereo aI[6480];
    SampleStereo aJ[3660];
    SampleStereo aK[1720];
    SampleStereo aL[680];

    SampleStereo aA[9700];
    SampleStereo aB[6000];
    SampleStereo aC[2320];
    SampleStereo aD[940];

    SampleStereo aE[15220];
    SampleStereo aF[8460];
    SampleStereo aG[4540];
    SampleStereo aH[3200];

    double aML[3111];
    double aMR[3111];

    SampleStereo feedbackA;
    SampleStereo feedbackB;
    SampleStereo feedbackC;
    SampleStereo feedbackD;

    SampleStereo lastRef[7];
    SampleStereo thunder;
    double oldfpd;

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

    double vibM;

    uint32_t fpdL;
    uint32_t fpdR;

    // internal constants, calculated every process call in original,
    // but i'd rather only recalculate them when necessary
    double regen, attenuate, lowpass, drift, size, wet;
    int cycleEnd;

    static constexpr auto A = 0;
    static constexpr auto B = 1;
    static constexpr auto C = 2;
    static constexpr auto D = 3;
    static constexpr auto E = 4;
    float param[paramCount] = {};

    void update()
    {
        double overallscale = 1.0 / 44100.0 * sr;

        cycleEnd = floor(overallscale);
        if (cycleEnd < 1) cycleEnd = 1;
        if (cycleEnd > 4) cycleEnd = 4;
        // this is going to be 2 for 88.1 or 96k, 3 for silly people, 4 for 176 or 192k
        if (cycle > cycleEnd-1) cycle = cycleEnd - 1; // sanity check

        regen = 0.0625 + ((1.0 - param[A]) * 0.0625);
        attenuate = (1.0 - (regen / 0.125)) * 1.333;
        lowpass = pow(1.00001 - (1.0 - param[B]), 2.0) / sqrt(overallscale);
        drift = pow(param[C], 3) * 0.001;
        size = (param[D] * 1.77) + 0.1;
        wet = 1.0 - pow(1.0 - param[E], 3);

        delayI = 3407.0 * size;
        delayJ = 1823.0 * size;
        delayK = 859.0  * size;
        delayL = 331.0  * size;
        delayA = 4801.0 * size;
        delayB = 2909.0 * size;
        delayC = 1153.0 * size;
        delayD = 461.0  * size;
        delayE = 7607.0 * size;
        delayF = 4217.0 * size;
        delayG = 2269.0 * size;
        delayH = 1597.0 * size;
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

        iirA = 0.0;
        iirB = 0.0;

        for (auto &n : aI) { n = 0; }
        for (auto &n : aJ) { n = 0; }
        for (auto &n : aK) { n = 0; }
        for (auto &n : aL) { n = 0; }
        for (auto &n : aA) { n = 0; }
        for (auto &n : aB) { n = 0; }
        for (auto &n : aC) { n = 0; }
        for (auto &n : aD) { n = 0; }
        for (auto &n : aE) { n = 0; }
        for (auto &n : aF) { n = 0; }
        for (auto &n : aG) { n = 0; }
        for (auto &n : aH) { n = 0; }
        for (auto &n : aML) { n = 0; }
        for (auto &n : aMR) { n = 0; }

        feedbackA = 0.0;
        feedbackB = 0.0;
        feedbackC = 0.0;
        feedbackD = 0.0;

        for (auto &n : lastRef) { n = 0; }
        thunder = 0;

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

    virtual const char *getName() { return "Galactic"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return param[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

    virtual void process(SampleStereo *in, size_t n_samples)
    {
        ZoneScopedN("Galactic");
        for (size_t i = 0; i < n_samples; i++) {
            auto inputSample = in[i];
            if (abs(inputSample.l) < 1.18e-23) inputSample.l = fpdL * 1.18e-17;
            if (abs(inputSample.r) < 1.18e-23) inputSample.r = fpdR * 1.18e-17;
            auto drySample = inputSample;

            vibM += (oldfpd * drift);
            if (vibM > pi) {
                vibM = -pi;
                oldfpd = 0.4294967295 + (fpdL * 0.0000000000618);
            }

            aML[countM] = inputSample.l * attenuate;
            aMR[countM] = inputSample.r * attenuate;
            countM++;
            if (countM < 0 || countM > delayM) countM = 0;

            auto xl = vibM;
            auto xr = vibM + pi / 2.0;
            if (xr > pi) xr -= pi * 2.0;
            auto x = SampleStereo(xl, xr);
            auto sin_x = fast_sin(x);
            auto offsetM = (sin_x + 1.0) * 127.0;
            int workingML = countM + offsetM.l;
            int workingMR = countM + offsetM.r;
            auto t = offsetM - floor(offsetM);
            // fixme: not pretty.
            SampleStereo y0 = { aML[workingML - ((workingML > delayM) ? delayM + 1 : 0)],
                                aMR[workingMR - ((workingMR > delayM) ? delayM + 1 : 0)] };
            SampleStereo y1 = { aML[workingML + 1 - ((workingML + 1 > delayM) ? delayM + 1 : 0)],
                                aMR[workingMR + 1 - ((workingMR + 1 > delayM) ? delayM + 1 : 0)] };
            auto interpolM = y0 * (SampleStereo(1.0) - t) + y1 * t;
            inputSample = interpolM;
            // predelay that applies vibrato
            // want vibrato speed AND depth like in MatrixVerb

            iirA = (iirA * (1.0 - lowpass)) + (inputSample * lowpass);
            inputSample = iirA;
            // initial filter

            cycle++;
            if (cycle == cycleEnd) { // hit the end point and we do a reverb sample
                aI[countI] = inputSample + (feedbackA * regen);
                aJ[countJ] = inputSample + (feedbackB * regen);
                aK[countK] = inputSample + (feedbackC * regen);
                aL[countL] = inputSample + (feedbackD * regen);

                countI++;
                if (countI < 0 || countI > delayI) countI = 0;
                countJ++;
                if (countJ < 0 || countJ > delayJ) countJ = 0;
                countK++;
                if (countK < 0 || countK > delayK) countK = 0;
                countL++;
                if (countL < 0 || countL > delayL) countL = 0;

                auto outI = aI[countI - ((countI > delayI) ? delayI + 1 : 0)];
                auto outJ = aJ[countJ - ((countJ > delayJ) ? delayJ + 1 : 0)];
                auto outK = aK[countK - ((countK > delayK) ? delayK + 1 : 0)];
                auto outL = aL[countL - ((countL > delayL) ? delayL + 1 : 0)];
                // first block: now we have four outputs

                aA[countA] = (outI - (outJ + outK + outL));
                aB[countB] = (outJ - (outI + outK + outL));
                aC[countC] = (outK - (outI + outJ + outL));
                aD[countD] = (outL - (outI + outJ + outK));

                countA++;
                if (countA < 0 || countA > delayA) countA = 0;
                countB++;
                if (countB < 0 || countB > delayB) countB = 0;
                countC++;
                if (countC < 0 || countC > delayC) countC = 0;
                countD++;
                if (countD < 0 || countD > delayD) countD = 0;

                auto outA = aA[countA - ((countA > delayA) ? delayA + 1 : 0)];
                auto outB = aB[countB - ((countB > delayB) ? delayB + 1 : 0)];
                auto outC = aC[countC - ((countC > delayC) ? delayC + 1 : 0)];
                auto outD = aD[countD - ((countD > delayD) ? delayD + 1 : 0)];
                // second block: four more outputs

                aE[countE] = (outA - (outB + outC + outD));
                aF[countF] = (outB - (outA + outC + outD));
                aG[countG] = (outC - (outA + outB + outD));
                aH[countH] = (outD - (outA + outB + outC));

                countE++;
                if (countE < 0 || countE > delayE) countE = 0;
                countF++;
                if (countF < 0 || countF > delayF) countF = 0;
                countG++;
                if (countG < 0 || countG > delayG) countG = 0;
                countH++;
                if (countH < 0 || countH > delayH) countH = 0;

                auto outE = aE[countE - ((countE > delayE) ? delayE + 1 : 0)];
                auto outF = aF[countF - ((countF > delayF) ? delayF + 1 : 0)];
                auto outG = aG[countG - ((countG > delayG) ? delayG + 1 : 0)];
                auto outH = aH[countH - ((countH > delayH) ? delayH + 1 : 0)];
                // third block: final outputs

                feedbackA = (outE - (outF + outG + outH));
                feedbackB = (outF - (outE + outG + outH));
                feedbackC = (outG - (outE + outF + outH));
                feedbackD = (outH - (outE + outF + outG));
                // which we need to feed back into the input again, a bit

                inputSample = (outE + outF + outG + outH) / 8.0;
                // and take the final combined sum of outputs
                if (cycleEnd == 1) {
                    lastRef[0] = inputSample;
                } else if (cycleEnd == 2) {
                    lastRef[0] = lastRef[2]; // start from previous last
                    lastRef[1] = (lastRef[0] + inputSample) / 2; // half
                    lastRef[2] = inputSample;                    // full
                } else if (cycleEnd == 3) {
                    lastRef[0] = lastRef[3]; // start from previous last
                    lastRef[2] = (lastRef[0] + lastRef[0] + inputSample) / 3;  // third
                    lastRef[1] = (lastRef[0] + inputSample + inputSample) / 3; // two thirds
                    lastRef[3] = inputSample;                                  // full
                } else if (cycleEnd == 4) {
                    lastRef[0] = lastRef[4]; // start from previous last
                    lastRef[2] = (lastRef[0] + inputSample) / 2; // half
                    lastRef[1] = (lastRef[0] + lastRef[2]) / 2;  // one quarter
                    lastRef[3] = (lastRef[2] + inputSample) / 2; // three quarters
                    lastRef[4] = inputSample;                    // full
                }
                cycle = 0; // reset
                inputSample = lastRef[cycle];
            } else {
                inputSample = lastRef[cycle];
                // we are going through our references now
            }

            iirB = (iirB * (1.0 - lowpass)) + (inputSample * lowpass);
            inputSample = iirB;
            // end filter

            inputSample = (inputSample * wet) + (drySample * (1.0 - wet));

            //begin 64 bit stereo floating point dither
            //int expon; frexp((double)inputSampleL, &expon);
            fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
            //inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
            //frexp((double)inputSampleR, &expon);
            fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
            //inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
            //end 64 bit stereo floating point dither

            in[i] = inputSample;
        }
    }
};

} // namespace rana::audio
