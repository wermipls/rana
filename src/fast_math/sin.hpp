#pragma once

#ifndef DOCTEST_CONFIG_DISABLE
    #include <doctest.h>
    #include <cmath>
    #include <bit>
    #include <numbers>
    #include "test_ulp.hpp"
#endif

namespace rana::fmath {

// Approximation of sin(x) on [-pi, pi].
// Uses similar method to https://mooooo.ooo/chebyshev-sine-approximation/
// but assumes double precision coefficients and arithmetic.
template <typename T>
static inline T sin(const T x)
{
    // coefficients generated with
    // ./lolremez --degree 5 --range "1e-50:pi*pi" "sin(sqrt(x))/sqrt(x)/(sqrt(x)-pi)/(sqrt(x)+pi)" "1/(sqrt(x))" --double
    const auto x2 = x * x;
           T u = T( 1.3132678020554575e-10);
    u = u * x2 + T(-2.3274725289039816e-08);
    u = u * x2 + T( 2.5218673277394069e-06);
    u = u * x2 + T(-0.00017350322264963387);
    u = u * x2 + T( 0.0066208762872755093);
    u = u * x2 + T(-0.10132118175052347);

    return (x - T(3.14159265358979311599796346854) + T(1.22464679914739511540935389571e-16)) *
           (x + T(3.14159265358979311599796346854) - T(1.22464679914739511540935389571e-16)) * u * x;
}

// Approximation of sin(x*pi) on [-1, 1].
// Coefficients taken from https://mooooo.ooo/chebyshev-sine-approximation/
// Assumes single precision arithmetic.
static inline float sinpi_f32(const float x)
{
    const float x2 = x * x;
       float y =  0.000385937753182769f;
    y = y * x2 + -0.006860187425683514f;
    y = y * x2 +  0.0751872634325299f;
    y = y * x2 + -0.5240361513980939f;
    y = y * x2 +  2.0261194642649887f;
    y = y * x2 + -3.1415926444234477f;
    return (x - 1.f) * (x + 1.f) * y * x;
}

// approximates sin(x*pi/2) on [-1, 1]
template <typename T>
static inline T sin_halfpi(const T x)
{
    // ./lolremez --degree 5 --range "1e-50:1" "sin(sqrt(x)*pi/2)/(sqrt(x))" "1/(sqrt(x)*pi/2)" --double
    // Degree 5 approximation of f(x) = sin(sqrt(x)*pi/2)/(sqrt(x))
    // with weight function g(x) = 1/(sqrt(x)*pi/2)
    // on interval [ 1e-50, 1 ]
    // p(x)=((((-3.4182130525186867e-6*x+1.6021724634303529e-4)*x-4.6816203508015543e-3)*x+7.9692587335035602e-2)*x-6.459640926526981e-1)*x+1.5707963266218764
    // Estimated max error: 2.0887105564869357e-11
    auto x2 = x*x;
           T u = T(-3.4182130525186866e-06);
    u = u * x2 + T( 0.00016021724634303529);
    u = u * x2 + T(-0.0046816203508015545);
    u = u * x2 + T( 0.079692587335035606);
    u = u * x2 + T(-0.64596409265269805);
    u = u * x2 + T( 1.5707963266218763);
    return u * x;
}

#ifndef DOCTEST_CONFIG_DISABLE

TEST_CASE("fmath::sin() - quick accuracy check") {
    // it's not realistic to test all cases, so just test some of them.
    // if i recall correctly, the max error is somewhere around 1 ULP
    // when rounded to single precision.
    for (uint32_t i = 0x00000000; i < 0x40490fdb; i += 0x100) {
        const double x = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::sin(x), std::sin(x)) <= 1);
        REQUIRE(fmath::sin(x) == -fmath::sin(-x)); // basic identity.
    }
}

TEST_CASE("fmath::sinpi_f32() - quick accuracy check") {
    // we know that max error is within 5 ULP.
    for (uint32_t i = 0x00000000; i < 0x3f800000; i += 0x100) {
        const double x = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::sinpi_f32(x), std::sin(x * std::numbers::pi)) <= 5);
        REQUIRE(fmath::sinpi_f32(x) == -fmath::sinpi_f32(-x)); // basic identity.
    }
}

TEST_CASE("fmath::sin_halfpi() - quick accuracy check") {
    // fuck knows what's the error, but given the amount of terms...
    for (uint32_t i = 0x00000000; i < 0x3f800000; i += 0x100) {
        const double x = std::bit_cast<float>(i);
        REQUIRE(ulp_f32(fmath::sin_halfpi(x), std::sin(x * std::numbers::pi / 2.0)) <= 1);
        REQUIRE(fmath::sin_halfpi(x) == -fmath::sin_halfpi(-x)); // basic identity.
    }
}

#endif // ifndef DOCTEST_CONFIG_DISABLE

} // namespace rana::fmath
