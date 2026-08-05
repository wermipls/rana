#pragma once

#include <stddef.h>

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

    void push(const T &v) { buf[(offset + sz) & mask] = v; offset++; }
    void push(T &&v)      { buf[(offset + sz) & mask] = v; offset++; }
};

} // namespace rana
