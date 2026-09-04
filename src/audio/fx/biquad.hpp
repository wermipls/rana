#pragma once

#include "audio/effect.hpp"
#include <cmath>
#include <tracy/Tracy.hpp>
#ifdef RANA_SUPERFLUOUS_VST_PARAMS
    #include <stdio.h>
#endif

namespace rana::audio {

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
        return (2. * pi*pi - 2 * vxy) / (pi*pi + vxy + pi * sqrt2 * sqrt(vxy + kxy));
    }

    static inline double phi2(double x, double y, double a)
    {
        auto vxy = v(x,y,a);
        auto kxy = k(x,y,a);
        return (pi*pi + vxy - pi * sqrt2 * sqrt(vxy + kxy)) / (pi*pi + vxy + pi * sqrt2 * sqrt(vxy + kxy));
    }

    static inline void recalculateCoeffs(Coeffs &coeff, double sr, Mode mode, double cutoff, double q, double gain_db)
    {
        cutoff = std::min(cutoff, sr / 2.0); // formulas aren't valid above nyquist.

        // FIXME: allow adjusting cutoff for channels separately.
        const double omega = sr / cutoff;
        const double damp = 0.5 / q;
        const double gain = std::pow(10.0, gain_db / 20.0);
        
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
            case Bandpass:
                a1 = phi_zero + phi_inf;
                a2 = phi_zero * phi_inf;
                b1 = phi1(omega, damp, a);
                b2 = phi2(omega, damp, a);
                G = omega / (2.f * pi) / (2 + a1);
                break;
            case FixedBandpass:
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
                a1 = phi1(omega * std::pow(gain, 0.25), damp, a);
                a2 = phi2(omega * std::pow(gain, 0.25), damp, a);
                b1 = phi1(omega * std::pow(gain, -0.25), damp, a);
                b2 = phi2(omega * std::pow(gain, -0.25), damp, a);
                G = 1.f / (1 + a1 + a2);
                break;
            case LowShelf:
                a1 = phi1(omega * std::pow(gain, -0.25), damp, a);
                a2 = phi2(omega * std::pow(gain, -0.25), damp, a);
                b1 = phi1(omega * std::pow(gain, 0.25), damp, a);
                b2 = phi2(omega * std::pow(gain, 0.25), damp, a);
                G = gain / (1 + a1 + a2);
                break;
            case Peaking:
                a1 = phi1(omega, damp * std::pow(gain, 0.5), a);
                a2 = phi2(omega, damp * std::pow(gain, 0.5), a);
                b1 = phi1(omega, damp * std::pow(gain, -0.5), a);
                b2 = phi2(omega, damp * std::pow(gain, -0.5), a);
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
    void getParamFmt(int index, char *str, size_t sz)
    {
        switch (index) {
            case 0: {
                const char *modestr[] = {
                    "Lowpass",
                    "Highpass",
                    "Bandpass",
                    "Fixed Bandpass",
                    "Notch",
                    "Allpass",
                    "Peaking",
                    "Low Shelf",
                    "High Shelf",
                };
                snprintf(str, sz, modestr[mode]);
                break;
            }
            case 1: snprintf(str, sz, "%f", cutoff.target); break;
            case 2: snprintf(str, sz, "%f", q.target); break;
            case 3: snprintf(str, sz, "%f", gain_db.target); break;
        }
    }

    static const char *getParamLabel(int index)
    {
        const char *labels[] = {
            "",
            "Hz",
            "Q",
            "dB",
        };
        static_assert(std::size(labels) == paramCount);
        return labels[index % paramCount];
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
            case 1: cutoff = std::pow(value, 3) * (22050.0f - 20.f) + 20.0f; break;
            case 2: q = std::pow(value, 3) * 29.9f + 0.1f; break;
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

} // namespace rana::audio
