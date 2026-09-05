#pragma once

#include "audio/common.hpp"
#include "audio/effect.hpp"
#include "log.hpp"
#include <bit>
#include <vector>

namespace rana::audio {

// Reverb design based on Geraint Luff's blog post:
// https://signalsmith-audio.co.uk/writing/2021/lets-write-a-reverb/
class Reverb2 : public Effect {
    static constexpr auto kParamCount = 12;
    const char *kParamNames[kParamCount] = {
        "mix",
        "width",
        "predelay",
        "size",
        "delay",
        "feedback",
        "mod amount",
        "mod speed",
        "fb lo gain",
        "fb hi gain",
        "lowpass",
        "highpass",
    };
    union {
        float by_index[kParamCount];
        struct {
            float mix           = 0.25;
            float width         = 1.0; // FIXME: unhandled
            float predelay      = 0.0;
            float size          = 0.5;
            float delay         = 0.2;
            float feedback      = 0.5;
            float mod_amount    = 0.2; // FIXME: unhandled
            float mod_speed     = 0.2; // FIXME: unhandled
            float fb_lo_gain    = 0.0; // FIXME: unhandled
            float fb_hi_gain    = 0.0; // FIXME: unhandled
            float lowpass       = 1.0;
            float highpass      = 0.0;
        };
    } param;

    template <size_t n>
    static void hadamard(double *in) {
        static_assert(n >= 2 && std::popcount(n) == 1); // must be power of 2.

        // this code is perhaps TOO dense.
        // the idea is we work with closest pairs first and double the gap
        // with every iteration. the indices we process go like so, for n=8:
        //       j=0  j=2  j=4  j=6
        // i=1   A B  A B  A B  A B     k --> 0 0  2 2  4 4  6 6
        // i=2   A A  B B  A A  B B     k --> 0 1  0 1  4 5  4 5
        // i=4   A A  A A  B B  B B     k --> 0 1  2 3  0 1  2 3
        for (size_t i = 1; i < n; i *= 2) {
            for (size_t j = 0; j < n; j += i*2) {
                for (size_t k = j; k < j+i; k++) {
                    double a = in[k];
                    double b = in[k + i];
                    in[k] = (a + b);
                    in[k + i] = (a - b);
                }
            }
        }

        const auto rsqrt_n = std::sqrt(1.0 / n);
        for (size_t i = 0; i < n; i++) {
            in[i] *= rsqrt_n;
        }
    }

    template <size_t n>
    static void householder(double *in) {
        static_assert(n >= 1);

        double sum = 0;
        for (size_t i = 0; i < n; i++) {
            sum += in[i];
        }
        sum *= -2.0 / n;
        for (size_t i = 0; i < n; i++) {
            in[i] += sum;
        }
    }

    template <size_t n, size_t invert_mask, size_t shuffle_increment>
    struct Diffuser {
        static_assert(n >= 2 && std::popcount(n) == 1); // must be power of 2.
        std::vector<double> delay[n];
        int delay_idx[n] = {};

        void process(double *in) {
            double out[n];
            const auto n_mask = n - 1;
            for (size_t i = 0; i < n; i++) {
                assert(delay[i].size() > 0);

                auto idx = delay_idx[i];
                const auto v = delay[i][idx]; // FIXME: we can modulate idx here.
                delay[i][idx] = in[i];
                // shuffle + invert.
                const auto invert = (invert_mask & (1<<i));
                const auto i_shuffled = (i + shuffle_increment) & n_mask;
                out[i_shuffled] = invert ? -v : v;

                idx--;
                if (idx < 0) {
                    idx = delay[i].size() - 1;
                }
                delay_idx[i] = idx;
            }

            hadamard<n>(out);
            memcpy(in, out, sizeof(out));
        }
    };

    static constexpr auto kMaxPredelaySeconds = 0.5;
    float sr;
    std::vector<SampleStereo> predelay;
    unsigned int predelay_idx = 0;
    Diffuser<8, 0b10101010, 1> diffuse1 = {};
    Diffuser<8, 0b11111000, 1> diffuse2 = {};
    Diffuser<8, 0b00010001, 1> diffuse3 = {};
    Diffuser<8, 0b00101110, 1> diffuse4 = {};
    std::vector<double> feedback[8];
    int feedback_idx[8] = {};
    SampleStereo lp_coef;
    SampleStereo hp_coef;
    SampleStereo lp = 0;
    SampleStereo hp = 0;

    bool need_update;

    void update() {
        for (size_t i = 0; i < 8; i++) {
            diffuse1.delay[i].resize(std::max<size_t>(diffuse1.delay[i].capacity() * param.size, 1));
            diffuse2.delay[i].resize(std::max<size_t>(diffuse2.delay[i].capacity() * param.size, 1));
            diffuse3.delay[i].resize(std::max<size_t>(diffuse3.delay[i].capacity() * param.size, 1));
            diffuse4.delay[i].resize(std::max<size_t>(diffuse4.delay[i].capacity() * param.size, 1));
            feedback[i].resize(std::max<size_t>(feedback[i].capacity() * param.delay, 1));
        }

        lp_coef = factor_lowpass_single_pole(20.0 + std::pow(param.lowpass, 3) * 22030.0, sr);
        hp_coef = factor_lowpass_single_pole(20.0 + std::pow(param.highpass, 3) * 22030.0, sr);

        need_update = false;
    }

public:
    Reverb2(float sample_rate = 44100)
        : sr{sample_rate}
        , predelay(kMaxPredelaySeconds * sample_rate)
    {
        const unsigned int some_primes[32] = {
            191,  337,  479,  521,   571,   601,   653,   739,
            1009, 1279, 1301, 1361,  1489,  1523,  1847,  2069,
            3313, 3533, 3701, 4073,  4127,  4513,  4729,  5081,
            9007, 9851, 9973, 10007, 10733, 10949, 11447, 11981,
        };
        auto scale = 44100.0 / sr;

        for (size_t i = 0; i < 8; i++) {
            diffuse1.delay[i].reserve(some_primes[i*4+0] * scale);
            diffuse2.delay[i].reserve(some_primes[i*4+1] * scale);
            diffuse3.delay[i].reserve(some_primes[i*4+2] * scale);
            diffuse4.delay[i].reserve(some_primes[i*4+3] * scale);
            feedback[i].reserve((i + 1) * sr / 8.0);
            // just to be sure. we rely on the vector being a good boy
            // and allocating exactly the specified amount.
            assert(diffuse1.delay[i].capacity() == some_primes[i*4+0] * scale);
            assert(diffuse2.delay[i].capacity() == some_primes[i*4+1] * scale);
            assert(diffuse3.delay[i].capacity() == some_primes[i*4+2] * scale);
            assert(diffuse4.delay[i].capacity() == some_primes[i*4+3] * scale);
        }
    }

    virtual const char *getName()           { return "Reverb2"; }
    virtual void setParam(int i, float v)   { param.by_index[i] = v; need_update = true; }
    virtual float getParam(int i)           { return param.by_index[i]; }
    virtual int getParamCount()             { return kParamCount; }
    virtual const char *getParamName(int i) { return kParamNames[i]; }

    virtual void process(SampleStereo *buf, size_t n) {
        if (need_update) {
            update();
        }

        const auto dry = SampleStereo(sqrt(1.0f - param.mix));
        const auto wet = SampleStereo(sqrt(param.mix));
        const size_t pre_samples = std::min<size_t>(param.predelay * kMaxPredelaySeconds * sr, predelay.size() - 1);
        double ch[8];

        for (size_t i = 0; i < n; i++) {
            const auto in = buf[i];

            // predelay.
            predelay_idx = (predelay_idx + 1) % predelay.size();
            predelay[predelay_idx] = in;
            const auto delayed_idx = (predelay_idx + predelay.size() - pre_samples) % predelay.size();
            assert(delayed_idx < predelay.size());
            const auto sample_delayed = predelay[delayed_idx];

            // diffuse.
            ch[0] = ch[2] = ch[4] = ch[6] = sample_delayed.l;
            ch[1] = ch[3] = ch[5] = ch[7] = sample_delayed.r;
            diffuse1.process(ch);
            diffuse2.process(ch);
            diffuse3.process(ch);
            diffuse4.process(ch);

            // delay.
            for (size_t j = 0; j < 8; j++) {
                ch[j] += feedback[j][feedback_idx[j]] * param.feedback;
            }
            householder<8>(ch);
            for (size_t j = 0; j < 8; j++) {
                auto idx = feedback_idx[j];
                feedback[j][idx] = ch[j];

                idx--;
                if (idx < 0) {
                    idx = feedback[j].size() - 1;
                }
                feedback_idx[j] = idx;
            }
            const auto diffused = SampleStereo(
                (ch[0] + ch[2] + ch[4] + ch[6]) * 0.25,
                (ch[1] + ch[3] + ch[5] + ch[7]) * 0.25
            );

            lp += (diffused - lp) * lp_coef;
            hp += (lp - hp) * hp_coef;
            const auto filtered = lp - hp;

            buf[i] = in * dry + filtered * wet;
        }
    }
};


} // namespace rana::audio
