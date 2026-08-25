#pragma once

#include "exp.hpp"
#include "log.hpp"
#include <doctest.h>
#ifndef DOCTEST_CONFIG_DISABLE
    #include <cmath>
    #include <numbers>
    #include "test_ulp.hpp"
#endif

namespace rana::fmath {

// approximates x^y for positive x.
// result is undefined if x is negative or there are NaNs.
// accuracy is limited by approximations of `ln()` and `exp()`.
static inline double pow(double x, double y)
{
    return exp(ln(x) * y);
}


TEST_CASE("fmath::pow() - basic test") {
    CHECK(fmath::pow(1.0, 1.0) == 1.0);
    CHECK(fmath::pow(1.0, -1.0) == 1.0);

    CHECK(fmath::pow(0.0, -1.0) == INFINITY);
    CHECK(fmath::pow(0.0, -INFINITY) == INFINITY);
    CHECK(fmath::pow(0.0, 2.0) == 0.0);

    CHECK(fmath::pow(1.0, 2137.0) == 1.0);
    CHECK(fmath::pow(67.0, 0.0) == 1.0);

    CHECK(fmath::pow(0.5, -INFINITY) == INFINITY);
    CHECK(fmath::pow(2.0, -INFINITY) == 0.0);
    CHECK(fmath::pow(0.5, INFINITY) == 0.0);
    CHECK(fmath::pow(2.0, INFINITY) == INFINITY);

    CHECK(fmath::pow(INFINITY, -2.0) == 0.0);
    CHECK(fmath::pow(INFINITY, 2.0) == INFINITY);
}

TEST_CASE("fmath::pow() - quick accuracy check") {
    // check some more or less common bases.
    auto x = GENERATE(0.1, 0.5, 1.0, 2.0, std::numbers::e, 10.0);

    // it's not realistic to test all cases, so just test some of them.
    // error is not specified but is dependent on ln and exp approximations,
    // so let's assume up to 16 ULP of error in single precision.
    for (uint32_t i = 0x00000000; i < 0x7f800000; i += 0x1000) {
        const double y = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::pow(x, y), std::pow(x, y)) <= 16);
    }
    // same for negatives.
    for (uint32_t i = 0x80000000; i < 0xff800000; i += 0x1000) {
        const double y = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::pow(x, y), std::pow(x, y)) <= 16);
    }
}

} // rana::fmath
