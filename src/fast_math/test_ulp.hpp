#pragma once

#include <stdint.h>
#include <cmath>
#include <format>
#include <bit>
#include <doctest.h>

namespace rana::fmath {
namespace detail {

template <typename T>
struct ResultULP {
    int64_t error;
    T a, b;

    operator int64_t() const {
        return error;
    };
};

template <typename T>
static doctest::String toString(const ResultULP<T> &in) {
    if (in.error == INT64_MAX) {
        return std::format("ulp({}, {}) -> inf", in.a, in.b);
    } else {
        return std::format("ulp({}, {}) -> {}", in.a, in.b, in.error);
    }
}

} // namespace detail

inline static detail::ResultULP<double> ulp(double a, double b)
{
    if (std::isnan(a) || std::isnan(b)) {
        if (std::isnan(a) && std::isnan(b)) {
            return detail::ResultULP{0, a, b};
        } else {
            return detail::ResultULP{INT64_MAX, a, b};
        }
    }

    int64_t x = std::bit_cast<int64_t>(a);
    int64_t y = std::bit_cast<int64_t>(b);

    // remap to one's complement so we get a contiguous range from -inf to inf.
    if (x & 0x8000000000000000) x ^= 0x7fffffffffffffff;
    if (y & 0x8000000000000000) y ^= 0x7fffffffffffffff;

    const auto error = std::abs(x - y);
    return detail::ResultULP{error, a, b};
}

inline static detail::ResultULP<float> ulp_f32(float a, float b)
{
    if (std::isnan(a) || std::isnan(b)) {
        if (std::isnan(a) && std::isnan(b)) {
            return detail::ResultULP{0, a, b};
        } else {
            return detail::ResultULP{INT64_MAX, a, b};
        }
    }

    int32_t x = std::bit_cast<int32_t>(a);
    int32_t y = std::bit_cast<int32_t>(b);

    // remap to one's complement so we get a contiguous range from -inf to inf.
    if (x & 0x80000000) x ^= 0x7fffffff;
    if (y & 0x80000000) y ^= 0x7fffffff;

    const auto error = std::abs(x - y);
    return detail::ResultULP{error, a, b};
}

TEST_CASE("ulp() - single precision self test") {
    CHECK(ulp_f32(1.0, 1.0 + FLT_EPSILON) == 1);
    CHECK(ulp_f32(1.0, 1.0 - FLT_EPSILON) == 2);
    CHECK(ulp_f32(FLT_TRUE_MIN, 0.0) == 1);
    CHECK(ulp_f32(-FLT_TRUE_MIN, FLT_TRUE_MIN) == 3);
    CHECK(ulp_f32(-1.0, -1.0 + FLT_EPSILON) == 2);
    CHECK(ulp_f32(-1.0, -1.0 - FLT_EPSILON) == 1);
    CHECK(ulp_f32(0.5, 1.0) == (1ull << 23));
    CHECK(ulp_f32(NAN, 0) == INT64_MAX);
    CHECK(ulp_f32(0, NAN) == INT64_MAX);
    CHECK(ulp_f32(NAN, NAN) == 0);
};

TEST_CASE("ulp() - double precision self test") {
    CHECK(ulp(1.0, 1.0 + DBL_EPSILON) == 1);
    CHECK(ulp(1.0, 1.0 - DBL_EPSILON) == 2);
    CHECK(ulp(DBL_TRUE_MIN, 0.0) == 1);
    CHECK(ulp(-DBL_TRUE_MIN, DBL_TRUE_MIN) == 3);
    CHECK(ulp(-1.0, -1.0 + DBL_EPSILON) == 2);
    CHECK(ulp(-1.0, -1.0 - DBL_EPSILON) == 1);
    CHECK(ulp(0.5, 1.0) == (1ull << 52));
    CHECK(ulp(1, 1.0 + FLT_EPSILON) == (1ull << 29));
    CHECK(ulp(1, 1.0 - FLT_EPSILON / 2.0) == (1ull << 29));
    CHECK(ulp(NAN, 0) == INT64_MAX);
    CHECK(ulp(0, NAN) == INT64_MAX);
    CHECK(ulp(NAN, NAN) == 0);
};

}
