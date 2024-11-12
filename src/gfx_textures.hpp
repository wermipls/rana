#pragma once
#include "types.hpp"

namespace rana {
namespace gfx {

uint32_t load_texture(const char *fn);
void destroy_texture(uint32_t tex);

}
}
