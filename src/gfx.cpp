#include "gfx.hpp"
#include "sdl_error.hpp"

namespace rana {
namespace gfx {

Context::Context(std::string title, int w, int h)
{
    if (SDL_CreateWindowAndRenderer(w, h, 0, &window, &renderer)) {
        sdl_error("failed to create window");
        throw;
    };

    SDL_SetWindowTitle(window, title.c_str());
}

Context::~Context()
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

}
}
