#pragma once

#include <cmath>

// fixme: move this elsewhere.
#if defined(__x86_64__) || defined(_M_X64)
    #define RANA_SSE2
#endif

#ifdef RANA_SSE2
    #include <immintrin.h>
#endif

namespace rana {
namespace audio {

constexpr double pi = 3.14159265358979323846;
constexpr double sqrt2 = 1.4142135623730950488016887242097;
typedef double Hz;

// This implementation is inspired by a blog post from Ralph Brorsen:
// https://revivalizer.xyz/articles/art-of-softsynth-development-using-sse-in-c-plus-plus-without-the-hassle/
//
// Some differences, in order to preserve my sanity:
// - The default constructor handles initialization (to avoid accidental UB).
// - Construction from a singular double is not explicit.
//   Typical usecase is for inline constants, which become unwieldy otherwise.
//   Codegen wise, MSVC and Clang appear to always duplicate the constant,
//   while GCC insists on using unpcklpd/movddup regardless of intrinsic.
union SampleStereo {
    struct {
        double l, r;
    };
#ifdef RANA_SSE2
    __m128d v;

    SampleStereo() : v() {}
    SampleStereo(const double x) : v(_mm_set1_pd(x)) {}
    SampleStereo(const __m128d &x) : v(x) {}
    SampleStereo(const SampleStereo &x) : v(x.v) {}
    SampleStereo(const double l, const double r) : v(_mm_set_pd(r, l)) {} // yes, the other way around.

    SampleStereo &operator=(const SampleStereo &rhs) { v = rhs.v; return *this; }

    SampleStereo operator+(const SampleStereo rhs) const { return SampleStereo(_mm_add_pd(v, rhs.v)); }
    SampleStereo operator-(const SampleStereo rhs) const { return SampleStereo(_mm_sub_pd(v, rhs.v)); }
    SampleStereo operator*(const SampleStereo rhs) const { return SampleStereo(_mm_mul_pd(v, rhs.v)); }
    SampleStereo operator/(const SampleStereo rhs) const { return SampleStereo(_mm_div_pd(v, rhs.v)); }

    SampleStereo &operator+=(const SampleStereo &rhs) { v = _mm_add_pd(v, rhs.v); return *this; }
    SampleStereo &operator-=(const SampleStereo &rhs) { v = _mm_sub_pd(v, rhs.v); return *this; }
    SampleStereo &operator*=(const SampleStereo &rhs) { v = _mm_mul_pd(v, rhs.v); return *this; }
    SampleStereo &operator/=(const SampleStereo &rhs) { v = _mm_div_pd(v, rhs.v); return *this; }
#else
    SampleStereo() : l(), r() {}
    SampleStereo(const double x) : l(x), r(x) {}
    SampleStereo(const SampleStereo &x) : l(x.l), r(x.r) {}
    SampleStereo(const double l, const double r) : l(l), r(r) {}

    SampleStereo &operator=(const SampleStereo &rhs) { l = rhs.l; r = rhs.r; return *this; }

    SampleStereo operator+(const SampleStereo rhs) const { return { l + rhs.l, r + rhs.r }; }
    SampleStereo operator-(const SampleStereo rhs) const { return { l - rhs.l, r - rhs.r }; }
    SampleStereo operator*(const SampleStereo rhs) const { return { l * rhs.l, r * rhs.r }; }
    SampleStereo operator/(const SampleStereo rhs) const { return { l / rhs.l, r / rhs.r }; }

    SampleStereo &operator+=(const SampleStereo &rhs) { *this = *this + rhs; return *this; }
    SampleStereo &operator-=(const SampleStereo &rhs) { *this = *this - rhs; return *this; }
    SampleStereo &operator*=(const SampleStereo &rhs) { *this = *this * rhs; return *this; }
    SampleStereo &operator/=(const SampleStereo &rhs) { *this = *this / rhs; return *this; }
#endif
};


static inline SampleStereo max(const SampleStereo a, const SampleStereo b)
{
#ifdef RANA_SSE2
    return _mm_max_pd(a.v, b.v);
#else
    return { std::max(a.l, b.l), std::max(a.r, b.r) };
#endif
}

static inline SampleStereo min(const SampleStereo a, const SampleStereo b)
{
#ifdef RANA_SSE2
    return _mm_min_pd(a.v, b.v);
#else
    return { std::min(a.l, b.l), std::min(a.r, b.r) };
#endif
}

static inline SampleStereo abs(const SampleStereo a)
{
#ifdef RANA_SSE2
    const auto mask = _mm_set1_pd(-0.0);
    return _mm_andnot_pd(mask, a.v);
#else
    return { std::abs(a.l), std::abs(a.r) };
#endif
}

static inline SampleStereo clamp(const SampleStereo a, const SampleStereo min_bound, const SampleStereo max_bound)
{
    return min(max(a, min_bound), max_bound);
}

static inline SampleStereo copysign(const SampleStereo value, const SampleStereo sign)
{
#ifdef RANA_SSE2
    const auto mask = _mm_set1_pd(-0.0);
    const auto abs_value = _mm_andnot_pd(mask, value.v);
    const auto sign_only = _mm_and_pd(mask, sign.v);
    return _mm_or_pd(sign_only, abs_value);
#else
    return { std::copysign(sign.l, value.l), std::copysign(sign.r, value.r) };
#endif
}

static inline SampleStereo tanh(const SampleStereo a)
{
    return { std::tanh(a.l), std::tanh(a.r) };
}

static inline SampleStereo pow(const SampleStereo a, const SampleStereo b)
{
    return { std::pow(a.l, b.l), std::pow(a.r, b.r) };
}

static inline SampleStereo fmod(const SampleStereo a, const SampleStereo b)
{
    return { std::fmod(a.l, b.l), std::fmod(a.r, b.r) };
}

static inline SampleStereo floor(const SampleStereo a)
{
    return { std::floor(a.l), std::floor(a.r) };
}

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
template <typename T>
static inline T fast_sin_halfpi(T x)
{
    // ./lolremez --degree 5 --range "1e-50:1" "sin(sqrt(x)*pi/2)/(sqrt(x))" "1/(sqrt(x)*pi/2)" --double
    // Degree 5 approximation of f(x) = sin(sqrt(x)*pi/2)/(sqrt(x))
    // with weight function g(x) = 1/(sqrt(x)*pi/2)
    // on interval [ 1e-50, 1 ]
    // p(x)=((((-3.4182130525186867e-6*x+1.6021724634303529e-4)*x-4.6816203508015543e-3)*x+7.9692587335035602e-2)*x-6.459640926526981e-1)*x+1.5707963266218764
    // Estimated max error: 2.0887105564869357e-11
    auto x2 = x*x;
    T u = -3.4182130525186866e-06;
    u = u * x2 + 0.00016021724634303529;
    u = u * x2 + -0.0046816203508015545;
    u = u * x2 + 0.079692587335035606;
    u = u * x2 + -0.64596409265269805;
    u = u * x2 + 1.5707963266218763;
    return u * x;
}

// approximates tanh(x).
// Formula taken from https://yaikhom.com/2020-04-28-localised-approximation-of-hyperbolic-tangents.html
// Estimated max error: 9.61e-5 at x=4.97179
template <typename T>
static inline T fast_tanh(T x)
{
    const auto x2 = x * x;
    auto a = x2 + 378.0;
    auto b = x2 * 28.0 + 3150.0;
         a = x2 * a + 17325.0;
         b = x2 * b + 62370.0;
         a = x2 * a + 135135.0;
         b = x2 * b + 135135.0;
         a *= x;
    return clamp(a / b, -1.0, 1.0);
}

}
}