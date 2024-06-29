#include "gfx.hpp"
#include "sdl_error.hpp"

namespace rana {
namespace gfx {

Context::Context(const char *title, int w, int h)
{
    if (SDL_CreateWindowAndRenderer(title, w, h, 0, &window, &renderer)) {
        sdl_error("failed to create window");
        throw;
    };
}

Context::~Context()
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

}
}
