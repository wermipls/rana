#pragma once

#include "audio/effect.hpp"
#include <memory>
#include <tracy/Tracy.hpp>

namespace rana::audio {

class Reverb : public Effect {
    static constexpr auto paramCount = 7;
    const char *paramNames[paramCount] = {
        "wet",
        "dry",
        "width",
        "roomsize",
        "damp",
        "lowpass",
        "highpass",
    };
    float params[paramCount] = {
        INITIALWET,
        INITIALDRY,
        INITIALWIDTH,
        INITIALROOM,
        INITIALDAMP,
        INITIALLOWPASS,
        INITIALHIPASS,
    };

    static constexpr auto NUMCOMBS       = 8;
    static constexpr auto NUMALLPASSES   = 4;
    static constexpr auto FIXEDGAIN      = 0.015;
    static constexpr auto SCALEWET       = 3.0;
    static constexpr auto SCALEDRY       = 2.0;
    static constexpr auto SCALEDAMP      = 0.4;
    static constexpr auto SCALEROOM      = 0.28;
    static constexpr auto STEREOSPREAD   = 23;
    static constexpr auto OFFSETROOM     = 0.7;
    static constexpr auto INITIALROOM    = 0.5;
    static constexpr auto INITIALDAMP    = 0.5;
    static constexpr auto INITIALWET     = 0.25;
    static constexpr auto INITIALDRY     = 0.5;
    static constexpr auto INITIALWIDTH   = 1.0;
    static constexpr auto INITIALLOWPASS = 1.0;
    static constexpr auto INITIALHIPASS  = 0.0;
    static constexpr auto INITIALSR      = 44100.0;

    struct Comb {
        double feedback = 0;
        double filterstore = 0;
        double damp1 = 0;
        double damp2 = 0;
        std::unique_ptr<double[]> buf;
        unsigned int bufsize = 0;
        unsigned int bufidx = 0;

        Comb() = default;

        Comb(unsigned int size) {
            buf = std::make_unique<double[]>(size);
            bufsize = size;
            memset(buf.get(), 0, sizeof(double) * size);
        }

        double process(double input) {
            double output = buf[bufidx];
            filterstore = output * damp2 + filterstore * damp1;
            buf[bufidx] = input + filterstore * feedback;

            if (++bufidx >= bufsize) {
                bufidx = 0;
            }

            return output;
        }

        void set_damp(double n) {
            damp1 = n;
            damp2 = 1.0 - n;
        }
    };

    struct Allpass {
        double feedback = 0.5;
        std::unique_ptr<double[]> buf;
        unsigned int bufsize = 0;
        unsigned int bufidx = 0;

        Allpass() = default;

        Allpass(unsigned int size) {
            buf = std::make_unique<double[]>(size);
            bufsize = size;
            memset(buf.get(), 0, sizeof(double) * size);
        }

        double process(double input) {
            double bufout = buf[bufidx];
            double output = -input + bufout;
            buf[bufidx] = input + bufout * feedback;

            if (++bufidx >= bufsize) {
                bufidx = 0;
            }

            return output;
        }
    };

    bool freeze_mode;
    double gain;
    double roomsize, roomsize1;
    double damp, damp1;
    double wet, wet1, wet2;
    double dry;
    double width;
    double sr;
    SampleStereo hp, hp_coeff;
    SampleStereo lp, lp_coeff;
    double hp_cutoff, lp_cutoff;
    bool need_update;

    Comb combl[NUMCOMBS];
    Comb combr[NUMCOMBS];
    Allpass allpassl[NUMALLPASSES];
    Allpass allpassr[NUMALLPASSES];

    void update()
    {
        wet1 = wet * (width * 0.5 + 0.5);
        wet2 = wet * ((1 - width) * 0.5);

        if (freeze_mode) {
            roomsize1 = 1;
            damp1 = 0;
            gain = 0;
        } else {
            roomsize1 = roomsize;
            damp1 = damp;
            gain = FIXEDGAIN;
        }

        for (int i = 0; i < NUMCOMBS; i++) {
            combl[i].feedback = roomsize1;
            combr[i].feedback = roomsize1;
            combl[i].set_damp(damp1);
            combr[i].set_damp(damp1);
        }

        lp_coeff = factor_lowpass_single_pole(lp_cutoff, sr);
        hp_coeff = factor_lowpass_single_pole(hp_cutoff, sr);

        need_update = false;
    }

    void set_samplerate(double value)
    {
        sr = value;

        const int combs[] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        const int allpasses[] = { 556, 441, 341, 225 };
        static_assert(_countof(combs) == NUMCOMBS);
        static_assert(_countof(allpasses) == NUMALLPASSES);

        double multiplier = value / INITIALSR;

        /* init comb buffers */
        for (size_t i = 0; i < NUMCOMBS; i++) {
            combl[i] = Comb(combs[i] * multiplier);
            combr[i] = Comb((combs[i] + STEREOSPREAD) * multiplier);
        }

        /* init allpass buffers */
        for (int i = 0; i < NUMALLPASSES; i++) {
            allpassl[i] = Allpass(allpasses[i] * multiplier);
            allpassr[i] = Allpass((allpasses[i] + STEREOSPREAD) * multiplier);
        }

        need_update = true;
    }

    void set_roomsize(double value) { roomsize = value * SCALEROOM + OFFSETROOM;  need_update = true; }
    void set_damp(double value)     { damp = value * SCALEDAMP;                   need_update = true; }
    void set_wet(double value)      { wet = value * SCALEWET;                     need_update = true; }
    void set_dry(double value)      { dry = value * SCALEDRY; }
    void set_width(double value)    { width = value;                              need_update = true; }
    void set_lowpass(double value)  { lp_cutoff = value * value * 19980. + 20.;   need_update = true; }
    void set_highpass(double value) { hp_cutoff = value * value * 19980. + 20.;   need_update = true; }

public:
    Reverb(float sample_rate = 44100) : sr{sample_rate}
    {
        set_samplerate(sr);
        set_wet(INITIALWET);
        set_roomsize(INITIALROOM);
        set_dry(INITIALDRY);
        set_damp(INITIALDAMP);
        set_width(INITIALWIDTH);
        set_highpass(INITIALHIPASS);
        set_lowpass(INITIALLOWPASS);
        update();
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: set_wet(value); break;
            case 1: set_dry(value); break;
            case 2: set_width(value); break;
            case 3: set_roomsize(value); break;
            case 4: set_damp(value); break;
            case 5: set_lowpass(value); break;
            case 6: set_highpass(value); break;
        }
    }

    virtual const char *getName() { return "Reverb"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Reverb");
        if (need_update) {
            update();
        }

        for (size_t i = 0; i < n; i++) {
            const auto s = in[i];
            SampleStereo out = {};
            double input_mono = (s.l + s.r) * gain;

            /* accumulate comb filters in parallel */
            for (int i = 0; i < NUMCOMBS; i++) {
                out.l += combl[i].process(input_mono);
                out.r += combr[i].process(input_mono);
            }

            /* feed through allpasses in series */
            for (int i = 0; i < NUMALLPASSES; i++) {
                out.l = allpassl[i].process(out.l);
                out.r = allpassr[i].process(out.r);
            }

            /* process hp/lp */
            hp += (out - hp) * hp_coeff;
            out -= hp;
            lp += (out - lp) * lp_coeff;
            out = lp;

            /* replace buffer with output */
            in[i].l = out.l * wet1 + out.r * wet2 + s.l * dry;
            in[i].r = out.r * wet1 + out.l * wet2 + s.r * dry;
        }
    }
};

} // namespace rana::audio
