#pragma once

#include "common.hpp"
#include "serializer.hpp"
#include <tracy/Tracy.hpp>
#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    #include <stdio.h>
    #include <string.h>
#endif
#include <memory>
#include "containers/bitmask_ringbuf.hpp"

namespace rana {
namespace audio {

using std::min, std::max, std::pow, std::fmod, std::abs, std::floor;

class Effect {
public:
    virtual void process(SampleStereo *in, size_t n) = 0;
    virtual void setParam(int index, float value) = 0;
    virtual float getParam(int index) { (void)index; return NAN; }
    virtual int getParamCount() { return 0; }
    virtual const char *getParamName(int index) { (void)index; return "n/a"; }
    virtual const char *getName() { return "Effect"; }
    virtual void setBPM(float bpm, bool retrigger = true) { (void)bpm; (void)retrigger; }
    virtual ~Effect() = default;

    virtual bool isSerializable() { return false; }
    void serialize(Serializer &s) { (void)s; }
};

class Filter1Pole : public Effect {
protected:
    static constexpr auto paramCount = 1;
    const char *paramNames[paramCount] = {
        "cutoff",
    };
    float params[paramCount] = { 0.5 };

    Hz sr;
    SampleStereo coeff = 0;
    SampleStereo q{};

    inline float normalized2coeff(float value)
    {
        // we want the filter value to reach 1 on the extreme edge
        // so you can set it so it doesn't affect the sound when doing LP,
        // rather than be truly accurate to -3dB frequency
        float coeff = factor_1pole(pow(value, 2) * 19980 + 20, sr);
        return min(coeff + pow(value, 30.0f), 1.0f);
    }

    Filter1Pole(Hz sample_rate = 44100)
    {
        sr = sample_rate;
        setCutoff(3000);
    }
    void setCutoff(Hz freq)
    {
        coeff = factor_1pole(freq, sr);
    }

public:
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    inline void getParamFmt(int index, char *str) {
        auto hz = std::acos(1 - coeff.l*coeff.l / (2-2*coeff.l)) * sr / (2 * pi);
        if (std::isnormal(hz)) {
            snprintf(str, 8, "%f", hz);
        } else {
            snprintf(str, 8, "> %f", sr/2);
        }
    }
    static inline void getParamLabel(int index, char *str) { strncpy(str, "Hz", 8); }
#endif

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: coeff = normalized2coeff(value); break;
        }
    }
};

class Lowpass : public Filter1Pole {
public:
    Lowpass(Hz sample_rate = 44100) : Filter1Pole(sample_rate) {}
    virtual const char *getName() { return "Lowpass"; }
    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Lowpass");
        for (size_t i = 0; i < n; i++) {
            q += (in[i] - q) * coeff;
            in[i] = q;
        }
    }
};

class Highpass : public Filter1Pole {
public:
    Highpass(Hz sample_rate = 44100) : Filter1Pole(sample_rate) {}
    virtual const char *getName() { return "Highpass"; }
    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Highpass");
        for (size_t i = 0; i < n; i++) {
            q += (in[i] - q) * coeff;
            in[i] -= q;
        }
    }
};

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
        double feedback = 0;
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

    static inline double filter_coeff(double cutoff_hz, double sr)
    {
        double freq = cutoff_hz * 2.0 * pi / sr;
        double y = 1. - cos(freq);
        return -y + sqrt(y*y + 2.0*y);
    }

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

        lp_coeff = filter_coeff(lp_cutoff, sr);
        hp_coeff = filter_coeff(hp_cutoff, sr);

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
            allpassl[i] = Allpass(combs[i] * multiplier);
            allpassr[i] = Allpass((combs[i] + STEREOSPREAD) * multiplier);
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
        for (int i = 0; i < NUMALLPASSES; i++) {
            allpassl[i].feedback = 0.5;
            allpassr[i].feedback = 0.5;
        }

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

class Delay : public Effect {
    static constexpr auto paramCount = 4;
    const char *paramNames[paramCount] = {
        "wet",
        "dry",
        "feedback",
        "delay",
    };
    float params[paramCount] = {
        0.25,
        1.0,
        0.25,
        0.1,
    };

    static constexpr auto max_delay_seconds = 5.0;
    std::unique_ptr<SampleStereo[]> buffer;
    size_t buffer_size;
    size_t buffer_pos = 0;
    size_t delay_size = 0;
    SampleStereo feedback = 0.25;
    SampleStereo wet = 0.25;
    SampleStereo dry = 1.0;

public:
    Delay(Hz sample_rate = 44100)
    {
        buffer_size = sample_rate * max_delay_seconds;
        buffer = std::make_unique<SampleStereo[]>(buffer_size);
        for (size_t i = 0; i < buffer_size; i++) {
            buffer[i] = 0;
        }

        setDelay(0.2);
    }

    void setDelay(float value)
    {
        if (value > 1.0) value = 1.0;
        if (value < 0.0) value = 0.0;

        delay_size = buffer_size * value;
        if (delay_size == 0) delay_size = 1;
        if (delay_size > buffer_size) delay_size = buffer_size;

        buffer_pos = buffer_pos % delay_size;
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: wet = value; break;
            case 1: dry = value; break;
            case 2: feedback = value; break;
            case 3: setDelay(value); break;
        }
    }

    virtual const char *getName() { return "Delay"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    inline void getParamFmt(int index, char *str) {
        switch (index) {
            default: snprintf(str, 8, "%f", params[index]); break;
            case 3: snprintf(str, 8, "%f", params[3] * max_delay_seconds * 1000.0); break;
        }
    }
    static inline void getParamLabel(int index, char *str) {
        switch (index) {
            default: str[0] = 0; break;
            case 3: strncpy(str, "ms", 8); break;
        }
    }
#endif

    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Delay");
        for (size_t i = 0; i < n; i++) {
            buffer_pos++;
            buffer_pos = buffer_pos % delay_size;
            auto delay_sample = buffer[buffer_pos];
            buffer[buffer_pos] = in[i] + delay_sample * feedback;
            in[i] = in[i] * dry + delay_sample * wet;
        }
    }
};

class Distortion : public Effect {
    static constexpr auto paramCount = 3;
    const char *paramNames[paramCount] = {
        "amount",
        "mix",
        "mode",
    };
    float params[paramCount] = {
        0.0,
        1.0,
        0.0,
    };

    static constexpr float gain_multi = 127;
    enum Mode {
        Hardclip,
        Softclip,
        Softsine,
        Tanh,
        Shape,
        Fold,
        BadFold,
    } mode;
    SampleStereo gain = 1;
    SampleStereo dry = 0;
    SampleStereo wet = 1;

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
        int i = value * (float)BadFold;
        mode = (Mode)i;
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: setAmount(value); break;
            case 1: setMix(value); break;
            case 2: setMode(value); break;
        }
    }

    virtual const char *getName() { return "Distortion"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    inline void getParamFmt(int index, char *str) {
        const char *mode_str[] = {
            "Hardclip",
            "Softclip",
            "Softsine",
            "Tanh",
            "Shape",
            "Fold",
            "BadFold"
        };
        switch (index) {
            default: snprintf(str, 8, "%f", params[index]); break;
            case 0: snprintf(str, 8, "%f", gain.l); break;
            case 2: snprintf(str, 8, "%s", mode_str[mode]); break;
        }
    }
    static inline void getParamLabel(int index, char *str) {
        switch (index) {
            default: str[0] = 0; break;
        }
    }
#endif

    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Distortion");
        switch (mode) 
        {
        case Hardclip:
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                in[i] = old * dry + max(min(in[i] * gain, 1.0), -1.0) * wet;
            }
            break;
        case Softclip: {
            constexpr auto a = -1.42479f;
            constexpr auto b =  2.20888f;
            constexpr auto c = -1.02786f;
            constexpr auto d =  1.13379f;
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                auto x = min(1.0, abs(in[i]) * gain);
                in[i] = old * dry + copysign((x*x*x*x*a + x*x*x*b + x*x*c + x*d), old) * wet;
            }
            break;
        }
        case Softsine:
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                auto x = min(1.0f, max(-1.0, old * gain / sqrt2));
                in[i] = old * dry + fast_sin_halfpi(x) * wet;
            }
            break;
        case Tanh:
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                in[i] = old * dry + fast_tanh(old * gain) * wet;
            }
            break;
        case Shape:
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                auto x = copysign(max(min(pow(abs(old), SampleStereo(1.0) / gain), 1.0), -1.0), old);
                in[i] = old * dry + x * wet;
            }
            break;
        case Fold:
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                auto a = (old * gain - 1.0) / 4.0;
                auto x = abs(a - floor(a) - 0.5) * 4.0 - 1.0;
                in[i] = old * dry + x * wet;
            }
            break;
        case BadFold:
            for (size_t i = 0; i < n; i++) {
                auto old = in[i];
                auto x = fmod(old * gain, 1.0);
                in[i] = old * dry + x * wet;
            }
            break;
        }
    }
};

class Bitcrush : public Effect {
    static constexpr auto paramCount = 3;
    const char *paramNames[paramCount] = {
        "bits",
        "rate",
        "smoothing",
    };
    float params[paramCount] = {
        1,
        1,
        0,
    };

    int bits = 16;
    double sr, rate;
    double t = 0;
    double smoothing = 0;
    SampleStereo x0 = {0,0};
    SampleStereo x1 = {0,0};

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
        setRate(1);
    }

    void setBits(float value)
    {
        bits = float2int(value, 2, 16);
    }

    void setRate(double value)
    {
        rate = 44100.0 * pow(value, 2);
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: setBits(value); break;
            case 1: setRate(value); break;
            case 2: smoothing = value; break;
        }
    }

    virtual const char *getName() { return "Bitcrush"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    inline void getParamFmt(int index, char *str) {
        switch (index) {
            case 0: snprintf(str, 8, "%d", bits); break;
            case 1: snprintf(str, 8, "%f", rate); break;
            case 2: snprintf(str, 8, "%f", smoothing); break;
        }
    }
    static inline void getParamLabel(int index, char *str) {
        switch (index) {
            case 0: strncpy(str, "bits", 8); break;
            case 1: strncpy(str, "Hz", 8); break;
            default: str[0] = 0; break;
        }
    }
#endif

    virtual void process(SampleStereo *in, size_t n)
    {
        // FIXME: vectorize.
        ZoneScopedN("Bitcrush");
        double linear_amt = std::max(smoothing*2.0 - 1.0, 0.0);
        double shave_amt = std::min(smoothing*2.0, 1.0);
        for (size_t i = 0; i < n; i++) {
            // rate
            t += rate / sr;
            double s = 0;
            if (t >= 1.0) {
                t -= 1.0;
                s = t;
                x1 = x0;
                x0 = in[i];
            }

            // bitcrush
            int l0 = bitcrush(x0.l * 32768, bits);
            int r0 = bitcrush(x0.r * 32768, bits);
            int l1 = bitcrush(x1.l * 32768, bits);
            int r1 = bitcrush(x1.r * 32768, bits);

            s = 1.f - s * shave_amt;
            s = s * (1.f - linear_amt) + t * linear_amt;

            in[i].l = (l0 / 32768.0) * s + (l1 / 32768.0) * (1.0 - s);
            in[i].r = (r0 / 32768.0) * s + (r1 / 32768.0) * (1.0 - s);
        }
    }
};

class Compressor : public Effect {
    static constexpr auto paramCount = 5;
    const char *paramNames[paramCount] = {
        "threshold",
        "attack",
        "release",
        "ratio",
        "makeup",
    };
    float params[paramCount] = {
        0.8,
        0.2,
        0.5,
        0.5,
        0.0,
    };

    static constexpr float PeakSmoothingHz = 20.0f;
    double sr;
    double pp_coeff;
    double max = 0;
    double pp = 0;
    double attack_coeff = 0.0;
    double release_coeff = 0.0;
    double threshold_db = 0.8;
    double volume_actual = 1.0;
    double volume_target = 0;
    double makeup = 1;
    double ratio = 0.5;

    void peakToPeak(SampleStereo value)
    {
        max *= pp_coeff;
        auto value_abs = abs(value);
        max = std::max(max, value_abs.l);
        max = std::max(max, value_abs.r);
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
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: threshold_db = dB(std::pow(value, 3) * 0.999f + 0.001f); break;
            case 1: attack_coeff  = factor_1pole(1.0f + std::pow(1.0f - value, 10) * 22049.0f, sr); break;
            case 2: release_coeff = factor_1pole(0.1f + std::pow(1.0f - value, 10) * 999.9f, sr); break;
            case 3: ratio = value; break; 
            case 4: makeup = std::pow(value, 3) * 16.0f + 1.0f; break;
        }
    }

    virtual const char *getName() { return "Compressor"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

    virtual void process(SampleStereo *in, size_t n)
    {
        // FIXME: vectorize.
        ZoneScopedN("Compressor");
        for (size_t i = 0; i < n; i++) {
            peakToPeak(in[i]);
            auto delta_db = pp - threshold_db;
            auto target_db = 0;
            if (delta_db > 0.0f) target_db -= delta_db * ratio;
            volume_target = from_dB(target_db);
            volume_target *= makeup;
            processVolume();
            in[i] *= volume_actual;
        }
    }
};

class Compressor2 : public Effect {
    static constexpr auto paramCount = 7;
    const char *paramNames[paramCount] = {
        "threshold",
        "attack",
        "release",
        "ratio",
        "knee",
        "makeup",
        "lookahead",
    };
    float params[paramCount] = {
        0.8,
        0.2,
        0.5,
        0.5,
        0.1,
        0.0,
        0.0,
    };

    // for display only.
    double attack_time;
    double release_time;
    double lookahead_time;
    double makeup_db;

    static constexpr float PeakSmoothingHz = 20.0f;
    static constexpr double MaxLookaheadSeconds = 0.020;
    static constexpr size_t MaxLookaheadSamples = 192000 * MaxLookaheadSeconds; // assume 192k is max for the effect.
    double sr;
    double pp_coeff;
    double pp_lookahead_coeff = 1;
    double pp = 0;
    double attack_coeff = 0;
    double release_coeff = 0;
    double threshold_db = 0.8;
    double volume_actual = 1.0;
    double volume_target = 0;
    double makeup = 1;
    double ratio = 0.5;
    double knee = 0.01;
    size_t lookahead_samples = 0;

    BitmaskRingBuf<SampleStereo, MaxLookaheadSamples> delay_buf;
    BitmaskRingBuf<double, MaxLookaheadSamples> peak_buf;

    void peakToPeak(SampleStereo value)
    {
        pp *= pp_coeff;
        auto value_abs = abs(value);
        auto max = pp;
        max = std::max(max, value_abs.l);
        max = std::max(max, value_abs.r);
        pp = max;
    }

    double attack_ramp()
    {
        // fixme: this can be optimized and *really* should. it's horribly slow.
        // if we run into a case where:
        //  1) max() takes value of peak_buf[i],
        //  2) we have already processed that value,
        // we know that subsequent samples will be exactly the same as before
        // and can reuse calculations from previous iterations.
        double peak = peak_buf[MaxLookaheadSamples-1];
        for (size_t i = MaxLookaheadSamples; i > MaxLookaheadSamples - lookahead_samples; i--) {
            peak *= pp_lookahead_coeff;
            peak = max(peak_buf[i-1], peak);
        }
        return peak;
    }

    void processVolume()
    {
        if (volume_actual > volume_target) {
            volume_actual += (volume_target - volume_actual) * attack_coeff;
        } else {
            volume_actual += (volume_target - volume_actual) * release_coeff;
        }
    }

    static inline double dB(double volume)
    {
        return 20.0 * std::log10(volume);
    }

    static inline double from_dB(double a)
    {
        return std::pow(10, a/20);
    }

    static inline double factor_1pole_target(double target, double iterations)
    {
        static_assert(std::numeric_limits<double>::is_iec559); // we need div by zero to yield +inf.
        return std::pow(target, 1.0 / (1 + iterations));
    }

    static inline double smooth_min(double a, double b, double k)
    {
        auto x = max(k - abs(a - b), 0.0);
        return min(a, b) - x * x / (k * 4.0);
    }

public:
    Compressor2(float sample_rate = 44100) : sr{sample_rate}
    {
        pp_coeff = 1.0f - factor_1pole(PeakSmoothingHz, sr);
        pp_lookahead_coeff = 0;
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: threshold_db = value * 60.0 - 60.0; break;
            case 1: {
                attack_time = std::pow(value, 3) * 10.0;
                attack_coeff  = 1.0 - factor_1pole_target(from_dB(-30), sr * attack_time);
                break;
            }
            case 2: {
                release_time = std::pow(value, 3) * 10.0;
                release_coeff = 1.0 - factor_1pole_target(from_dB(-30), sr * release_time);
                break;
            }
            case 3: ratio = value * 1.25 - 0.25; break; 
            case 4: knee = value * (30.0 - 0.01) + 0.01; break;
            case 5: {
                makeup_db = value * 60.0;
                makeup = from_dB(makeup_db);
                break;
            }
            case 6: {
                lookahead_time = value * MaxLookaheadSeconds;
                lookahead_samples = lookahead_time * sr;
                pp_lookahead_coeff = factor_1pole_target(from_dB(-20.0), lookahead_samples);
                break;
            }
        }
    }

    virtual const char *getName() { return "Compressor2"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    inline void getParamFmt(int index, char *str)
    {
        switch (index) {
            case 0: snprintf(str, 8, "%f", threshold_db); break;
            case 1: snprintf(str, 8, "%f", attack_time * 1000.0); break;
            case 2: snprintf(str, 8, "%f", release_time * 1000.0); break;
            case 3: snprintf(str, 8, "%f", ratio); break;
            case 4: snprintf(str, 8, "%f", params[4]); break;
            case 5: snprintf(str, 8, "%f", makeup_db); break;
            case 6: snprintf(str, 8, "%f", lookahead_time * 1000.0); break;
        }
    }

    static inline void getParamLabel(int index, char *str)
    {
        switch (index) {
            case 0: strncpy(str, "dB", 8); break;
            case 1: strncpy(str, "ms", 8); break;
            case 2: strncpy(str, "ms", 8); break;
            case 3: strncpy(str, "", 8); break;
            case 4: strncpy(str, "", 8); break;
            case 5: strncpy(str, "dB", 8); break;
            case 6: strncpy(str, "ms", 8); break;
        }
    }
#endif

    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Compressor2");
        for (size_t i = 0; i < n; i++) {
            auto s = in[i];
            delay_buf.push(s);
            peakToPeak(s);
            peak_buf.push(pp);
            auto pp_db = dB(attack_ramp());
            auto delta_db = pp_db - threshold_db;
            auto comp_curve = smooth_min(threshold_db + delta_db * ratio, pp_db, knee);
            volume_target = from_dB(comp_curve - pp_db);
            processVolume();
            in[i] = delay_buf[MaxLookaheadSamples - lookahead_samples - 1] * volume_actual * makeup;
        }
    }
};

class Biquad : public Effect {
    static constexpr auto paramCount = 4;
    const char *paramNames[paramCount] = {
        "mode",
        "cutoff",
        "q",
        "gain",
    };
    float params[paramCount] = {
        0.0,
        0.5,
        0.2729,
        0.5,
    };

    double sr;
    struct Coeffs {
        SampleStereo a0, a1, a2, b0, b1, b2;
        SampleStereo y1 = {0,0};
        SampleStereo y2 = {0,0};
    } c;
    enum Mode : int {
        Lowpass,
        Highpass,
        Bandpass,
        FixedBandpass,
        Notch,
        Allpass,
        Peaking,
        LowShelf,
        HighShelf,
    } mode = Lowpass;
    Rampable cutoff;
    Rampable q;
    Rampable gain_db;

    // https://ivantsovy.com/research/paper1.pdf
    // FIXME: simplify all those calculations?
    static inline double phi(double x, double a)
    {
        const auto sq = sqrt(x*x + a*a);
        return (pi - sq) / (pi + sq);
    }

    static inline double v(double x, double y, double a)
    {
        return sqrt(x*x*x*x + 2.*a*a*x*x * (2. * y*y - 1.) + a*a*a*a);
    }

    static inline double k(double x, double y, double a)
    {
        return x*x * (2. * y*y - 1.) + a*a;
    }

    static inline double phi1(double x, double y, double a)
    {
        auto vxy = v(x,y,a);
        auto kxy = k(x,y,a);
        return (2.f * pi*pi - 2 * vxy) / (pi*pi + vxy + pi * sqrt2 * sqrt(vxy + kxy));
    }

    static inline double phi2(double x, double y, double a)
    {
        auto vxy = v(x,y,a);
        auto kxy = k(x,y,a);
        return (pi*pi + vxy - pi * sqrt2 * sqrt(vxy + kxy)) / (pi*pi + vxy + pi * sqrt2 * sqrt(vxy + kxy));
    }

    static inline void recalculateCoeffs(Coeffs &coeff, float sr, Mode mode, float cutoff, float q, float gain_db)
    {
        // FIXME: allow adjusting cutoff for channels separately.
        const double omega = sr / cutoff;
        const double damp = 0.5 / q;
        const double gain = pow(10.0f, gain_db / 20.0);
        
        const double ctg_pi_omega = tan(pi/2 - (pi/omega));
        const double a = sqrt(omega*omega - pi*pi * ctg_pi_omega*ctg_pi_omega); // (2.11)
        
        const double phi_zero = (pi - a) / (pi + a);
        const double phi_inf = -1.f;

        double a1, a2, b1, b2, G;
        switch (mode) {
            default:
            case Lowpass:
                a1 = phi_zero + phi_zero;
                a2 = phi_zero * phi_zero;
                b1 = phi1(omega, damp, a);
                b2 = phi2(omega, damp, a);
                G = 1.f / (1 + a1 + a2);
                break;
            case Highpass:
                a1 = phi_inf + phi_inf;
                a2 = phi_inf * phi_inf;
                b1 = phi1(omega, damp, a);
                b2 = phi2(omega, damp, a);
                G = omega*omega / (4.f*pi*pi);
                break;
            case FixedBandpass:
            case Bandpass: // FIXME
                a1 = phi_zero + phi_inf;
                a2 = phi_zero * phi_inf;
                b1 = phi1(omega, damp, a);
                b2 = phi2(omega, damp, a);
                G = omega / (2.f * pi) * 2 * damp / (2 + a1);
                break;
            case Notch: // or "Band Stop". simplified coefficients from (3.9)
                a1 = -2.f * (omega*omega - a*a - pi*pi) / (omega*omega - a*a + pi*pi);
                a2 = 1;
                b1 = phi1(omega, damp, a);
                b2 = phi2(omega, damp, a);
                G = 1.f / (1 + a1 + a2);
                break;
            case HighShelf:
                a1 = phi1(omega * powf(gain, 0.25f), damp, a);
                a2 = phi2(omega * powf(gain, 0.25f), damp, a);
                b1 = phi1(omega * powf(gain, -0.25f), damp, a);
                b2 = phi2(omega * powf(gain, -0.25f), damp, a);
                G = 1.f / (1 + a1 + a2);
                break;
            case LowShelf:
                a1 = phi1(omega * powf(gain, -0.25f), damp, a);
                a2 = phi2(omega * powf(gain, -0.25f), damp, a);
                b1 = phi1(omega * powf(gain, 0.25f), damp, a);
                b2 = phi2(omega * powf(gain, 0.25f), damp, a);
                G = gain / (1 + a1 + a2);
                break;
            case Peaking:
                a1 = phi1(omega, damp * powf(gain, 0.5f), a);
                a2 = phi2(omega, damp * powf(gain, 0.5f), a);
                b1 = phi1(omega, damp * powf(gain, -0.5f), a);
                b2 = phi2(omega, damp * powf(gain, -0.5f), a);
                G = 1.f / (1 + a1 + a2);
                break;
            case Allpass:
                a1 = phi1(omega, damp, a) / phi2(omega, damp, a);
                a2 = 1.f / phi2(omega, damp, a);
                b1 = phi1(omega, damp, a);
                b2 = phi2(omega, damp, a);
                G = 1.f / (1 + a1 + a2);
                break;
        }

        double mul = G * (1.f + b1 + b2);
        coeff.b0 = mul;
        coeff.b1 = mul * a1;
        coeff.b2 = mul * a2;
        coeff.a0 = 1;
        coeff.a1 = b1;
        coeff.a2 = b2;
    }

public:
    Biquad(Hz sample_rate = 44100)
        : cutoff(5000, sample_rate)
        , q(0.5, sample_rate)
        , gain_db(0, sample_rate)
    {
        sr = sample_rate;
        recalculateCoeffs(c, sr, mode, cutoff.current, q.current, gain_db.current);
    }

    virtual const char *getName() { return "Biquad"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    inline void getParamFmt(int index, char *str)
    {
        switch (index) {
            case 0: {
                const char *modestr[] = {
                    "Lowpass",
                    "Highpass",
                    "Bandpass",
                    "FixedBandpass",
                    "Notch",
                    "Allpass",
                    "Peaking",
                    "LowShelf",
                    "HighShelf",
                };
                snprintf(str, 8, modestr[mode]);
                break;
            }
            case 1: snprintf(str, 8, "%f", cutoff.target); break;
            case 2: snprintf(str, 8, "%f", q.target); break;
            case 3: snprintf(str, 8, "%f", gain_db.target); break;
        }
    }

    static inline void getParamLabel(int index, char *str)
    {
        switch (index) {
            case 0: strncpy(str, "", 8); break;
            case 1: strncpy(str, "Hz", 8); break;
            case 2: strncpy(str, "Q", 8); break;
            case 3: strncpy(str, "dB", 8); break;
        }
    }
#endif

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: mode = Mode(value * (float)Mode::HighShelf);
                    recalculateCoeffs(c, sr, mode, cutoff.current, q.current, gain_db.current);
                    break;
            case 1: cutoff = pow(value, 3) * (22050.0f - 20.f) + 20.0f; break;
            case 2: q = pow(value, 3) * 29.9f + 0.1f; break;
            case 3: gain_db = -24.0f + value * 48.0f; break;
        }
    }

    virtual void process(SampleStereo *in, size_t n)
    {
        ZoneScopedN("Biquad");
        for (size_t i = 0; i < n; i++) {
            // recalculating coeffs is more expensive than just checking if the values are stable
            if (!cutoff.hasSettled() || !q.hasSettled() || !gain_db.hasSettled()){
                recalculateCoeffs(c, sr, mode, cutoff(), q(), gain_db());
            }

            SampleStereo y;
            const auto x = in[i];
            y = c.y1 + c.b0 * x;
            c.y1 = c.y2 + c.b1 * x - c.a1 * y;
            c.y2 = c.b2 * x - c.a2 * y;
            in[i] = y;
        }
    }
};

class TranceGate : public Effect {
    static constexpr auto paramCount = 5;
    const char *paramNames[paramCount] = {
        "dry/wet",
        "attack",
        "decay",
        "sustain",
        "release",
    };
    float params[paramCount] = {};

    float sr;
    float bpm = 120;

    float mix = 1;
    float attack  = 0.005;
    float decay   = 0.2;
    float sustain = 0.5;
    float release = 0.02;

    enum Note : uint8_t {
        Off    = 0,
        On     = 1,
        Legato = 2,
    };

    uint8_t pattern[64] = { 1,0,1,2,0,1,1,0, 1,0,1,2,0,1,1,0 };
    int pattern_length = 16;
    int pattern_pos = 0;
    float measure_length = 1;
    float samples_until_next = 0;

    bool is_release = true;
    float last = 0;
    float time_since_trigger = 0;
    float last_release = 0;
    float time_since_release = 0;

    void note()
    {
        switch (pattern[pattern_pos]) {
            case Note::On:
                time_since_trigger = 0;
                if (!is_release) {
                    time_since_release = 0;
                    last_release = last;
                }
                is_release = false;
                break;
            case Note::Legato:
                is_release = false;
                break;
            case Note::Off:
                if (!is_release) {
                    time_since_release = 0;
                    last_release = last;
                }
                is_release = true;
                break;
        }
        pattern_pos = (pattern_pos + 1) % pattern_length;
        samples_until_next = 60.0f / bpm * 4.0f * measure_length / (float)pattern_length * sr; // FIXME: actually calculate fragment time
    }

public:
    TranceGate(float sample_rate = 44100)
    {
        sr = sample_rate;
    }

    virtual void setParam(int index, float value)
    {
        if (index >= paramCount) return;
        params[index] = value;

        switch (index) {
            case 0: mix = value; break;
            case 1: attack = 0.001f + std::pow(value, 3) * 2.0f; break;
            case 2: decay = 0.001f + std::pow(value, 3) * 2.0f; break;
            case 3: sustain = value; break;
            case 4: release = 0.001f + std::pow(value, 3) * 2.0f; break;
        }
    }

    virtual const char *getName() { return "TranceGate"; }
    virtual const char *getParamName(int index) { return paramNames[index % paramCount]; }
    virtual float getParam(int index) { return params[index % paramCount]; }
    virtual int getParamCount() { return paramCount; }

    virtual void process(SampleStereo *in, size_t n)
    {
        for (size_t i = 0; i < n; i++) {
            if (samples_until_next <= 0.0f) {
                note();
            }

            float volume;
            float volume_release = (1.0f - min(1.0f, time_since_release / release)) * last_release;
            // adsr
            if (!is_release) {
                if (time_since_trigger <= attack) {
                    volume = time_since_trigger / attack;
                } else {
                    volume = 1.0f - min(1.0f, ((time_since_trigger - attack) / decay)) * (1.0f - sustain); 
                }
                last = volume;
                volume = max(volume, volume_release);
            } else {
                volume = volume_release;
            }

            volume = volume * volume;

            samples_until_next--;
            time_since_trigger += 1.0f / sr;
            time_since_release += 1.0f / sr;
            in[i] *= volume;
        }
    }
};

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

}
}
