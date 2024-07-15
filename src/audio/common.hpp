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

}
}