#pragma once

#include <stddef.h>

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

    void push(const T &v) { auto i = (offset + sz) & mask; buf[i] = v; buf[i+capacity/2] = v; offset = (offset+1) & mask; }
    void push(T &&v)      { auto i = (offset + sz) & mask; buf[i] = v; buf[i+capacity/2] = v; offset = (offset+1) & mask; }

    const T *data() { return &buf[offset]; }
};

} // namespace rana
