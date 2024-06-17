#pragma once

#include <SDL2/SDL.h>
#include "log.hpp"

namespace rana {

void sdl_error(const char *msg) {
    log::err("%s (%s)", msg, SDL_GetError());
}

}
