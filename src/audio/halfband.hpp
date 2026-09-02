#pragma once

#include "containers/linear_ringbuf.hpp"
#include <span>
#include <utility>
#include <assert.h>
#ifndef DOCTEST_CONFIG_DISABLE
    #include <doctest.h>
#endif

namespace rana::audio {

// 31-tap halfband FIR filter coefficients.
// Center coefficient is assumed to be 1.0.
// Transition width: 0.2
// Stopband attenuation: 53dB
template <typename T>
static constinit T halfband31_coefs[8] = {
    T(-0.003021832836918),
    T( 0.008264656077280),
    T(-0.017610212964013),
    T( 0.033080216934466),
    T(-0.058287722480607),
    T( 0.101985711138645),
    T(-0.196135136830915),
    T( 0.631111083939198),
};

// Assumes center coefficient is 1.0.
template <typename T, size_t taps>
static inline T halfband_decimate(const T *x, const T *coefs)
{
    static_assert(taps & 1, "tap count must be odd");

    constexpr size_t order = taps - 1;
    constexpr size_t wing_coefs = order / 4;

    T y = (x[0] + x[order]) * coefs[0];
    for (size_t i = 1; i <= wing_coefs; i++) {
        y += (x[i*2] + x[order-i*2]) * coefs[i];
    }
    y += x[order/2];
    return y * T(0.5);
}

// Assumes center coefficient is 1.0 and `x` is not zero stuffed.
template <typename T, size_t taps>
static inline T halfband_interpolate(const T *x, const T *coefs)
{
    static_assert(taps & 1, "tap count must be odd");

    constexpr size_t order = taps - 1;
    constexpr size_t wing_coefs = order / 4;

    T y = (x[0] + x[order/2]) * coefs[0];
    for (size_t i = 1; i <= wing_coefs; i++) {
        y += (x[i] + x[order/2-i]) * coefs[i];
    }
    return y;
}

template <typename T>
struct Downsampler2x {
    static constexpr auto latency = 7;
    LinearRingBuf<T, 31> rb;

    T process(T s0, T s1) {
        rb.push(s0);
        rb.push(s1);
        return halfband_decimate<T, 31>(rb.data(), halfband31_coefs<T>);
    }
};

template <typename T>
struct Upsampler2x {
    static constexpr auto latency = 8;
    LinearRingBuf<T, 16> rb;

    std::pair<T, T> process(T s) {
        rb.push(s);
        return { rb[8-1], halfband_interpolate<T, 31>(rb.data(), halfband31_coefs<T>)};
    }
};

// Combination of Upsampler2x and Downsampler2x with convenience functionality.
// It's dynamically toggleable, has latency reporting and can mix dry/wet signal.
// Motivating example:
//
//     Oversampler2x<float> os = {};
//     os.for_each({buf, buf+n}, [](float in) {
//         return tanh(in); // do some processing.
//     });
//
template <typename T>
class Oversampler2x {
    static constexpr auto _latency = Upsampler2x<T>::latency + Downsampler2x<T>::latency;

    Upsampler2x<T> _upsampler;
    Downsampler2x<T> _downsampler;
    T _mix = T(1.0);
    bool _enabled = true;

public:
    void enable(bool enabled = true) { _enabled = enabled; }
    bool enabled() const { return _enabled; }
    int latency() const { return _enabled ? _latency : 0; }
    void mix(T mix) { _mix = mix; }

    template <typename Callback>
    void for_each(std::span<T> range, Callback process_sample_cb) {
        const auto wet = _mix;
        const auto dry = T(1.0) - _mix;

        if (_enabled) {
            for (auto &n : range) {
                // dry sample needs to be latency compensated.
                // we reuse upsampler's buffer, since it already has the data we need.
                assert(_upsampler.rb.size() >= _latency); // can't use static_assert() until C++23.
                const auto sample_dry = _upsampler.rb[_upsampler.rb.size() - _latency];

                auto [s0, s1] = _upsampler.process(n);
                s0 = process_sample_cb(s0);
                s1 = process_sample_cb(s1);
                n = dry * sample_dry + wet * _downsampler.process(s0, s1);
            }
        } else {
            for (auto &n : range) {
                n = dry * n + wet * process_sample_cb(n);
            }
        }
    }
};

#ifndef DOCTEST_CONFIG_DISABLE

TEST_CASE("Upsampler2x") {
    double a[32];
    Upsampler2x<double> upsampler = {};

    auto [a0, a1] = upsampler.process(1.0);
    a[0] = a0;
    a[1] = a1;
    for (int i = 2; i < 31; i += 2) {
        auto [s0, s1] = upsampler.process(0.0);
        a[i+0] = s0;
        a[i+1] = s1;
    }

    CHECK(a[0] == 0.0);
    CHECK(a[1] == doctest::Approx(halfband31_coefs<double>[0]));
    CHECK(a[2] == 0.0);
    CHECK(a[3] == doctest::Approx(halfband31_coefs<double>[1]));
    CHECK(a[4] == 0.0);
    CHECK(a[5] == doctest::Approx(halfband31_coefs<double>[2]));
    CHECK(a[6] == 0.0);
    CHECK(a[7] == doctest::Approx(halfband31_coefs<double>[3]));
    CHECK(a[8] == 0.0);
    CHECK(a[9] == doctest::Approx(halfband31_coefs<double>[4]));
    CHECK(a[10] == 0.0);
    CHECK(a[11] == doctest::Approx(halfband31_coefs<double>[5]));
    CHECK(a[12] == 0.0);
    CHECK(a[13] == doctest::Approx(halfband31_coefs<double>[6]));
    CHECK(a[14] == 0.0);
    CHECK(a[15] == doctest::Approx(halfband31_coefs<double>[7]));
    CHECK(a[16] == 1.0);
    CHECK(a[17] == doctest::Approx(halfband31_coefs<double>[7]));
    CHECK(a[18] == 0.0);
    CHECK(a[19] == doctest::Approx(halfband31_coefs<double>[6]));
    CHECK(a[20] == 0.0);
    CHECK(a[21] == doctest::Approx(halfband31_coefs<double>[5]));
    CHECK(a[22] == 0.0);
    CHECK(a[23] == doctest::Approx(halfband31_coefs<double>[4]));
    CHECK(a[24] == 0.0);
    CHECK(a[25] == doctest::Approx(halfband31_coefs<double>[3]));
    CHECK(a[26] == 0.0);
    CHECK(a[27] == doctest::Approx(halfband31_coefs<double>[2]));
    CHECK(a[28] == 0.0);
    CHECK(a[29] == doctest::Approx(halfband31_coefs<double>[1]));
    CHECK(a[30] == 0.0);
    CHECK(a[31] == doctest::Approx(halfband31_coefs<double>[0]));

    // latency is in input samples, so multiply by 2.
    CHECK(a[upsampler.latency * 2] == 1.0);
}

TEST_CASE("Downsampler2x") {
    double a[16];
    Downsampler2x<double> downsampler = {};

    SUBCASE("unit impulse") {
        a[0] = downsampler.process(1.0, 0.0);
        for (int i = 1; i < 16; i++) {
            a[i] = downsampler.process(0.0, 0.0);
        }
        CHECK(a[0] == 0.0);
        CHECK(a[1] == 0.0);
        CHECK(a[2] == 0.0);
        CHECK(a[3] == 0.0);
        CHECK(a[4] == 0.0);
        CHECK(a[5] == 0.0);
        CHECK(a[6] == 0.0);
        CHECK(a[7] == 0.5);
        CHECK(a[8] == 0.0);
        CHECK(a[9] == 0.0);
        CHECK(a[10] == 0.0);
        CHECK(a[11] == 0.0);
        CHECK(a[12] == 0.0);
        CHECK(a[13] == 0.0);
        CHECK(a[14] == 0.0);
        CHECK(a[15] == 0.0);

        CHECK(a[downsampler.latency] == 0.5);
    }

    SUBCASE("unit impulse delayed by 1 sample") {
        a[0] = downsampler.process(0.0, 1.0);
        for (int i = 1; i < 16; i++) {
            a[i] = downsampler.process(0.0, 0.0);
        }

        CHECK(a[0] == doctest::Approx(halfband31_coefs<double>[0] / 2.0));
        CHECK(a[1] == doctest::Approx(halfband31_coefs<double>[1] / 2.0));
        CHECK(a[2] == doctest::Approx(halfband31_coefs<double>[2] / 2.0));
        CHECK(a[3] == doctest::Approx(halfband31_coefs<double>[3] / 2.0));
        CHECK(a[4] == doctest::Approx(halfband31_coefs<double>[4] / 2.0));
        CHECK(a[5] == doctest::Approx(halfband31_coefs<double>[5] / 2.0));
        CHECK(a[6] == doctest::Approx(halfband31_coefs<double>[6] / 2.0));
        CHECK(a[7] == doctest::Approx(halfband31_coefs<double>[7] / 2.0));
        CHECK(a[8] == doctest::Approx(halfband31_coefs<double>[7] / 2.0));
        CHECK(a[9] == doctest::Approx(halfband31_coefs<double>[6] / 2.0));
        CHECK(a[10] == doctest::Approx(halfband31_coefs<double>[5] / 2.0));
        CHECK(a[11] == doctest::Approx(halfband31_coefs<double>[4] / 2.0));
        CHECK(a[12] == doctest::Approx(halfband31_coefs<double>[3] / 2.0));
        CHECK(a[13] == doctest::Approx(halfband31_coefs<double>[2] / 2.0));
        CHECK(a[14] == doctest::Approx(halfband31_coefs<double>[1] / 2.0));
        CHECK(a[15] == doctest::Approx(halfband31_coefs<double>[0] / 2.0));
    }
}

TEST_CASE("Oversampler2x") {
    SUBCASE("latency reporting - enabled case") {
        double a[32] = {};
        a[0] = 1.0;
        Oversampler2x<double> os = {};
        os.for_each(a, [](double sample) {
            return sample; // doesn't really matter what we do here.
        });
    
        auto latency = os.latency();
        CHECK(latency > 0);
        CHECK(a[latency] == doctest::Approx(1.0).epsilon(0.1));
        for (int i = 0; i < 32; i++) {
            if (i == latency) continue;
            CHECK(a[i] == doctest::Approx(0.0).epsilon(0.1));
        }
    }

    SUBCASE("latency reporting - dry case (mix == 0)") {
        double a[32] = {};
        a[0] = 1.0;
        Oversampler2x<double> os = {};
        os.mix(0);
        os.for_each(a, [](double sample) {
            return sample; // doesn't really matter what we do here.
        });
    
        auto latency = os.latency();
        CHECK(latency > 0);
        CHECK(a[latency] == doctest::Approx(1.0).epsilon(0.1));
        for (int i = 0; i < 32; i++) {
            if (i == latency) continue;
            CHECK(a[i] == doctest::Approx(0.0).epsilon(0.1));
        }
    }

    SUBCASE("latency reporting - disabled case") {
        double a[32] = {};
        a[0] = 1.0;
        Oversampler2x<double> os = {};
        os.enable(false);
        os.for_each(a, [](double sample) {
            return sample; // doesn't really matter what we do here.
        });
    
        auto latency = os.latency();
        CHECK(latency == 0);
        CHECK(a[latency] == doctest::Approx(1.0).epsilon(0.1));
        for (int i = 0; i < 32; i++) {
            if (i == latency) continue;
            CHECK(a[i] == doctest::Approx(0.0).epsilon(0.1));
        }
    }
}

#endif // #ifndef DOCTEST_CONFIG_DISABLE

} // namespace rana::audio