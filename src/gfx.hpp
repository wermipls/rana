#pragma once

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_render.h>

namespace rana {
namespace gfx {

class Context {
public:
    Context(const char *title = "rana", int w = 1280, int h = 720);
    ~Context();

    void draw();

private:
    SDL_Window *window;
    SDL_GLContext glcontext;
    unsigned int shaderprog, vao;
};

}
}
