#pragma once

#include <cmath>
#include "fast_math/sin.hpp"

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

    // Apparently you can just mark the ctors as constexpr and it works on Clang 20+,
    // since the intrinsics are constexpr. Not necessarily the case on other compilers,
    // so we define some fallbacks instead, which seems wrong, but oh well.
#if __clang_major__ >= 20
    constexpr SampleStereo() : v(_mm_setzero_pd()) {}
    constexpr SampleStereo(const double x) : v(_mm_set1_pd(x)) {}
    constexpr SampleStereo(const __m128d &x) : v(x) {}
    constexpr SampleStereo(const SampleStereo &x) : v(x.v) {}
    constexpr SampleStereo(const double l, const double r) : v(_mm_setr_pd(l, r)) {}
#else
    constexpr SampleStereo() : v{0, 0} {}
    constexpr SampleStereo(const double x) : v{x,x} {}
    constexpr SampleStereo(const __m128d &x) : v(x) {}
    constexpr SampleStereo(const SampleStereo &x) : v(x.v) {}
    constexpr SampleStereo(const double l, const double r) : v{l,r} {}
#endif

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
    constexpr SampleStereo() : l(), r() {}
    constexpr SampleStereo(const double x) : l(x), r(x) {}
    constexpr SampleStereo(const SampleStereo &x) : l(x.l), r(x.r) {}
    constexpr SampleStereo(const double l, const double r) : l(l), r(r) {}

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

// 0 -> full left, 1 -> full right.
static inline SampleStereo pan_equal_power(float pan)
{
    return SampleStereo{
        sqrt2 * sqrt(1.0 - pan),
        sqrt2 * sqrt(pan),
    };
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

// cutoff should be in range [0, sr/2].
static inline double factor_lowpass_single_pole(Hz cutoff, Hz sr)
{
    // https://dsp.stackexchange.com/a/54088
    auto freq = cutoff / sr;
    auto y = 1.0 + fmath::sin_halfpi(4.0 * freq - 1.0);
    return sqrt(y*y + 2.0*y) - y;
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
        coef = factor_lowpass_single_pole(freq, sr);
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