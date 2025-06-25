#pragma once

#include <cmath>

namespace rana {
namespace audio {

constexpr double pi = 3.14159265358979323846;
constexpr double sqrt2 = 1.4142135623730950488016887242097;
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

// approximates sin(x*pi/2) over [-1, 1]
static double fast_sin_halfpi(double x)
{
    // ./lolremez --degree 5 --range "1e-50:1" "sin(sqrt(x)*pi/2)/(sqrt(x))" "1/(sqrt(x)*pi/2)" --double
    // Degree 5 approximation of f(x) = sin(sqrt(x)*pi/2)/(sqrt(x))
    // with weight function g(x) = 1/(sqrt(x)*pi/2)
    // on interval [ 1e-50, 1 ]
    // p(x)=((((-3.4182130525186867e-6*x+1.6021724634303529e-4)*x-4.6816203508015543e-3)*x+7.9692587335035602e-2)*x-6.459640926526981e-1)*x+1.5707963266218764
    // Estimated max error: 2.0887105564869357e-11
    auto x2 = x*x;
    double u = -3.4182130525186866e-06;
    u = u * x2 + 0.00016021724634303529;
    u = u * x2 + -0.0046816203508015545;
    u = u * x2 + 0.079692587335035606;
    u = u * x2 + -0.64596409265269805;
    u = u * x2 + 1.5707963266218763;
    return u * x;
}

}
}