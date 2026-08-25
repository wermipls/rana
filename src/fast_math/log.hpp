#pragma once

#include <stdint.h>
#include <doctest.h>
#ifndef DOCTEST_CONFIG_DISABLE
    #include <cmath>
    #include <numbers>
    #include "test_ulp.hpp"
#endif

namespace rana::fmath {

// approximates log2(x) on [ 1, 2 ].
static inline double log2_piecewise(double x)
{
    // $ ./lolremez --degree 8 -r 1+1e-50:2 --double "log2(x)/(x-1)" "1.00001-(x-1)"
    // coefficients perturbed by brute force.
    double k[] = {
         0.005835235613796637029,
        -0.08224624716288450599,
         0.5135471981230849137,
        -1.867896604322639709,
         4.377197236796875224,
        -6.906051655675589984,
         7.475042031201961201,
        -5.674808426876198908,
         3.60207616376361317
    };
         double u = k[0];
        u = u * x + k[1];
        u = u * x + k[2];
        u = u * x + k[3];
        u = u * x + k[4];
        u = u * x + k[5];
        u = u * x + k[6];
        u = u * x + k[7];
    return (u * x + k[8]) * (x - 1.0);
}

// approximates log2(x) on [ 0, inf ].
// max error: roughly 3.18ULP when rounded to single precision.
static inline double log2(double x)
{
    union fp64 {
        struct {
            uint64_t frac : 52;
            uint64_t exp  : 11;
            uint64_t sign : 1;
        } fpr;
        double d;
    };

    if (x <= 0) return -INFINITY;
    if (x == INFINITY) return INFINITY;

    fp64 a;
    a.d = __builtin_fabs(x);
    int exp = int(a.fpr.exp) - 1023;
    a.fpr.exp = 1023;

    auto log = exp + log2_piecewise(a.d);
    return log;
}

// approximates log10(x) on [ 0, inf ].
// accuracy is limited by approximation of `log2()`.
static inline double log10(double x)
{
    const auto log10_2 = 0.30102999566398119521373889472449;
    return log2(x) * log10_2;
}

// approximates ln(x) on [ 0, inf ].
// accuracy is limited by approximation of `log2()`.
static inline double ln(double x)
{
    const auto ln_2 = 0.69314718055994530941723212145818;
    return log2(x) * ln_2;
}


TEST_CASE("fmath::log2() - basic identities") {
    CHECK(log2(0.0) == -INFINITY);
    CHECK(log2(-0.0) == -INFINITY);
    CHECK(log2(1.0) == 0.0);
    CHECK(log2(INFINITY) == INFINITY);
    CHECK(log2(2.0) == 1.0);
}

TEST_CASE("fmath::ln() - basic identities") {
    CHECK(ln(0.0) == -INFINITY);
    CHECK(ln(-0.0) == -INFINITY);
    CHECK(ln(1.0) == 0.0);
    CHECK(ln(INFINITY) == INFINITY);
    // not enough accuracy for an exact match, but OK when rounded to f32.
    CHECK(ulp_f32(ln(std::numbers::e), 1.0) < 1);
}

TEST_CASE("fmath::log10() - basic identities") {
    CHECK(log10(0.0) == -INFINITY);
    CHECK(log10(-0.0) == -INFINITY);
    CHECK(log10(1.0) == 0.0);
    CHECK(log10(INFINITY) == INFINITY);
    // ditto.
    CHECK(ulp_f32(log10(10), 1.0) < 1);
}

TEST_CASE("fmath::log2() - quick accuracy check") {
    // it's not realistic to test all cases, so just test some of them.
    // function claims to be accurate to 3.18 ULP (when rounded to single).
    // let's assume error is within 4 ULP.
    for (uint32_t i = 0x00000000; i < 0x7f800000; i += 0x1000) {
        const double x = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::log2(x), std::log2(x)) <= 4);
    }

    // we know that accuracy gets worse around 1, so do extra checking there to be sure.
    for (uint32_t i = 0x3f800000 - 32; i < 0x3f800000 + 32; i++) {
        const double x = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::log2(x), std::log2(x)) <= 4);
    }
}

TEST_CASE("fmath::ln() - quick accuracy check") {
    // same as log2() variant.
    for (uint32_t i = 0x00000000; i < 0x7f800000; i += 0x1000) {
        const double x = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::ln(x), std::log(x)) <= 4);
    }

    for (uint32_t i = 0x3f800000 - 32; i < 0x3f800000 + 32; i++) {
        const double x = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::ln(x), std::log(x)) <= 4);
    }
}

TEST_CASE("fmath::log10() - quick accuracy check") {
    // same as log2() variant.
    for (uint32_t i = 0x00000000; i < 0x7f800000; i += 0x1000) {
        const double x = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::log10(x), std::log10(x)) <= 4);
    }

    for (uint32_t i = 0x3f800000 - 32; i < 0x3f800000 + 32; i++) {
        const double x = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::log10(x), std::log10(x)) <= 4);
    }
}


} // namespace rana::fmath
