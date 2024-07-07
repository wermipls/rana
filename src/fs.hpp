#pragma once

#include <vector>
#include <stdint.h>

namespace rana {
namespace fs {

void init(const char *argv0);
bool readfile(std::vector<uint8_t> &buffer, const char *fn);
size_t writefile(std::vector<uint8_t> &buffer, const char *fn);

}
}
