#pragma once

#include <string>
#include <SDL2/SDL.h>

namespace rana {
namespace gfx {

class Context {
public:
    Context(std::string title = "rana", int w = 1280, int h = 720);
    ~Context();

private:
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
};

}
}
