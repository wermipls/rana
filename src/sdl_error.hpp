#pragma once

#include <string>
#include <fmt/core.h>
#include <SDL2/SDL.h>

namespace rana {

void sdl_error(std::string msg) {
    fmt::println("{}: {}", SDL_GetError(), msg);
}

}
