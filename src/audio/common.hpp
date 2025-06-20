#pragma once

#include <cmath>

namespace rana {
namespace audio {

constexpr double pi = 3.14159265358979323846;
typedef double Hz;

struct SampleStereo {
    float l, r;
};

static inline double normalize_frequency(Hz freq, Hz sr)
{
    return freq * 2. * pi / sr; 
}

static inline double derive_1pole_factor(double freq)
{
    // assumes normalized angular frequency
    // https://dsp.stackexchange.com/a/54088

    double y = 1. - cos(freq);
    return -y + sqrt(y*y + 2*y);
}

static inline double factor_1pole(Hz freq, Hz sr)
{
    return derive_1pole_factor(normalize_frequency(freq, sr));
}

static inline SampleStereo pan_equal_power(float pan)
{
    pan += 1.f;
    pan *= pi / 4;

    return SampleStereo{
        std::sin(pan),
        std::cos(pan)
    };
}

struct Rampable {
    float current;
    float target;
    float coef;

    Rampable(float initial_value, Hz sr, Hz ramping_frequency = 100) {
        target = current = initial_value;
        setCoefficient(sr, ramping_frequency);
    }

    inline void setCoefficient(Hz sr, Hz freq) {
        coef = derive_1pole_factor(normalize_frequency(freq, sr));
    }
    inline void set(float x) { target = x; }
    inline void setInstant(float x) { target = current = x; }
    inline float step() {
        // do calculation on doubles so we can settle much closer to the target value
        return current = current + ((double)target - current) * coef;
    }
    inline bool hasSettled() {
        auto n = current + ((double)target - current) * coef;
        return (float)n == current; 
    }

    inline float operator()() { return step(); }
    inline float operator=(float rhs) { return target = rhs; }
};

}
}