#include "sdl_error.hpp"
#include <SDL3/SDL_error.h>
#include "log.hpp"

void rana::sdl_error(const char *msg) {
    rana::log::err("%s (%s)", msg, SDL_GetError());
}
