#pragma once

#include <cstdio>
#include <vector>
#include <stdint.h>

namespace rana {

bool readfile(std::vector<uint8_t> &buffer, const char *fn)
{
    auto f = std::fopen(fn, "rb");
    if (!f) {
        return false;
    }
    std::vector<uint8_t> buf(1024);
    size_t total_size = 0;
    while (auto size = fread(buf.data()+total_size, 1, 1024, f)) {
        total_size += size;
        buf.resize(buf.size() + 1024);
    }

    buf.resize(total_size);
    std::fclose(f);
    buffer = buf;
    return true; 
}

size_t writefile(std::vector<uint8_t> &buffer, const char *fn)
{
    auto f = std::fopen(fn, "wb");
    if (!f) {
        return 0;
    }
    auto bytes = std::fwrite(buffer.data(), 1, buffer.size(), f);
    std::fclose(f);
    return bytes;
}

}