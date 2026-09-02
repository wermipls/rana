#pragma once

#include <stddef.h>
#ifndef DOCTEST_CONFIG_DISABLE
    #include <doctest.h>
#endif

namespace rana {

// A ring buffer implementation that uses a bitmask for every buffer access.
template <typename T, size_t sz>
class BitmaskRingBuf {
    constexpr static size_t next_power_of_2(size_t n) {
        n--;
        for (size_t i = 1; i < sizeof(size_t) * 8; i <<= 1) {
            n |= n >> i;
        }
        n++;
        return n;
    }

    constexpr static size_t mask     = next_power_of_2(sz) - 1;
    constexpr static size_t capacity = next_power_of_2(sz);

    size_t offset;
    T buf[capacity];

public:
          T &operator[](size_t i)       { return buf[(i+offset) & mask]; }
    const T &operator[](size_t i) const { return buf[(i+offset) & mask]; }

    static constexpr size_t size() { return sz; }

    void push(const T &v) { buf[(offset + sz) & mask] = v; offset++; }
    void push(T &&v)      { buf[(offset + sz) & mask] = v; offset++; }
};

#ifndef DOCTEST_CONFIG_DISABLE

TEST_CASE("BitmaskRingBuf - smoke test") {
    BitmaskRingBuf<int, 5> rb = {};

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
