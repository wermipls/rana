#pragma once

#include <string>
#include <SDL2/SDL.h>
#include "log.hpp"

namespace rana {

void sdl_error(const std::string &msg) {
    log::err("{} ({})", msg, SDL_GetError());
}

}
