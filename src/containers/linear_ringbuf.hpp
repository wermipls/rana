#pragma once

#include <stddef.h>
#ifndef DOCTEST_CONFIG_DISABLE
    #include <doctest.h>
#endif

namespace rana {

// A ring buffer implementation providing zero-cost access at current offset.
// This is achieved by duplicating the buffers in memory, making writes slower.
template <typename T, size_t sz>
class LinearRingBuf {
    constexpr static size_t next_power_of_2(size_t n) {
        n--;
        for (size_t i = 1; i < sizeof(size_t) * 8; i <<= 1) {
            n |= n >> i;
        }
        n++;
        return n;
    }

    constexpr static size_t mask     = next_power_of_2(sz) - 1;
    constexpr static size_t capacity = next_power_of_2(sz) * 2;

    size_t offset; // fixme: perhaps replace this with pointer to buf+offset?
    T buf[capacity];

public:
    const T &operator[](size_t i) const { return buf[i+offset]; }

    static constexpr size_t size() { return sz; }

    void push(const T &v) { auto i = (offset + sz) & mask; buf[i] = v; buf[i+capacity/2] = v; offset = (offset+1) & mask; }
    void push(T &&v)      { auto i = (offset + sz) & mask; buf[i] = v; buf[i+capacity/2] = v; offset = (offset+1) & mask; }

    const T *data() { return &buf[offset]; }
};

#ifndef DOCTEST_CONFIG_DISABLE

TEST_CASE("LinearRingBuf - smoke test") {
    LinearRingBuf<int, 5> rb = {};

    for (int i = 0; i < 9; i++) {
        rb.push(i);
    }

    CHECK(rb[0] == 4);
    CHECK(rb[1] == 5);
    CHECK(rb[2] == 6);
    CHECK(rb[3] == 7);
    CHECK(rb[4] == 8);

    rb.push(42);

    CHECK(rb[0] == 5);
    CHECK(rb[1] == 6);
    CHECK(rb[2] == 7);
    CHECK(rb[3] == 8);
    CHECK(rb[4] == 42);
}

#endif // ifndef DOCTEST_CONFIG_DISABLE

} // namespace rana
