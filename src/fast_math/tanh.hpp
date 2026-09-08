#pragma once

#include "audio/common.hpp"
#include <algorithm>
#include <float.h>

#ifndef DOCTEST_CONFIG_DISABLE
    #include <doctest.h>
    #include <cmath>
    #include <bit>
    #include "test_ulp.hpp"
#endif

namespace rana::fmath {
namespace detail {

template <typename T>
static constexpr bool is_double_precision = T(1.0 + DBL_EPSILON) != T(1.0);

// sanity check.
static_assert(is_double_precision<double>);
static_assert(!is_double_precision<float>);

template <class T, class ClampFunc>
static inline T tanh_lambert7(T x, ClampFunc clamp)
{
    // We clamp the input early to avoid division by infinity later on.
    // Regrettably, a different constant is needed depending on precision,
    // assuming we want the outputs to stay within [ -1, 1 ].
    //
    // The single precision constant was found with linear search.
    // The double precision constant was found with binary search, then refined
    // with linear search around the result, since the function cannot be assumed
    // to be monotonically increasing.
    static constexpr double limit = is_double_precision<T> ? 4.971786858527149 : 4.971297;
    x = clamp(x, T(-limit), T(limit));

    const auto x2 = x * x;
    auto a = x2 + T(378);
         a = x2 * a + T(17325);
         a = x2 * a + T(135135);
         a *= x;
    auto b = T(28);
         b = x2 * b + T(3150);
         b = x2 * b + T(62370);
         b = x2 * b + T(135135);
    return a / b;
}

template <class T>
concept Clampable = requires(T a) {
    std::clamp(a, T(0), T(0));
};

} // namespace detail

// Approximation of tanh(x) on [ -inf, inf ].
// Error bounds (single precision arithmetic):
// - overall max error: 1613.3 ULP at x=4.971297
// - abs(x) <= 2: within 4 ULP
// - abs(x) >= 8: within 4 ULP
template <detail::Clampable T>
static inline T tanh(T x)
{
    return detail::tanh_lambert7(x, std::clamp<T>);
}

// Approximation of tanh(x) on [ -inf, inf ].
// Estimated max error: 9.62e-05 at x=4.971297
static inline audio::SampleStereo tanh(audio::SampleStereo x)
{
    return detail::tanh_lambert7(x, audio::clamp);
}

#ifndef DOCTEST_CONFIG_DISABLE

TEST_CASE("fmath::tanh() - clamps output to [ -1.0, 1.0 ]") {
    // we need this, otherwise the compiler folds the whole function into a constant.
    volatile float infinity = INFINITY;

    // those will most likely not pass with fused-multiply add.
    CHECK(fmath::tanh<float>(infinity) == 1.0);
    CHECK(fmath::tanh<float>(-infinity) == -1.0);
    CHECK(fmath::tanh<double>(infinity) == 1.0);
    CHECK(fmath::tanh<double>(-infinity) == -1.0);
}

TEST_CASE("fmath::tanh() - quick accuracy check") {
    SUBCASE("0 to 2") {
        auto start = std::bit_cast<uint32_t>(0.0f);
        auto end   = std::bit_cast<uint32_t>(2.0f);
        for (uint32_t i = start; i <= end; i += 0x100) {
            const float x = std::bit_cast<float>(i);
            REQUIRE(ulp_f32(fmath::tanh(x), std::tanh(x)) <= 4);
            REQUIRE(fmath::tanh(x) == -fmath::tanh(-x));
        }
    }

    SUBCASE("8 to infinity") {
        auto start = std::bit_cast<uint32_t>(8.0f);
        auto end   = std::bit_cast<uint32_t>(INFINITY);
        for (uint32_t i = start; i <= end; i += 0x100) {
            const float x = std::bit_cast<float>(i);
            auto tanh_approx = fmath::tanh(x);
            REQUIRE(ulp_f32(tanh_approx, std::tanh(x)) <= 4);
            REQUIRE(tanh_approx == -fmath::tanh(-x));
        }
    }

    SUBCASE("2 to 8") {
        auto start = std::bit_cast<uint32_t>(2.0f);
        auto end   = std::bit_cast<uint32_t>(8.0f);
        for (uint32_t i = start; i <= end; i += 0x100) {
            const float x = std::bit_cast<float>(i);
            REQUIRE(ulp_f32(fmath::tanh(x), std::tanh(x)) <= 1613);
            REQUIRE(fmath::tanh(x) == -fmath::tanh(-x));
        }
    }

    SUBCASE("detailed checking around 4.971297") {
        auto x_maxerr = std::bit_cast<uint32_t>(4.971297f);
        for (uint32_t i = x_maxerr - 0x100; i < x_maxerr + 0x100; i++) {
            const float x = std::bit_cast<float>(i);
            REQUIRE(ulp_f32(fmath::tanh(x), std::tanh(x)) <= 1613);
            REQUIRE(fmath::tanh(x) == -fmath::tanh(-x));
        }
    }
}

#endif // ifndef DOCTEST_CONFIG_DISABLE

}