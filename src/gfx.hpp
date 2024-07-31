#pragma once

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_render.h>
#include <glm/vec2.hpp>

namespace rana {
namespace gfx {

class Context {
public:
    Context(const char *title = "rana", int w = 1280, int h = 720);
    ~Context();

    void drawBegin();
    void drawFinish();

    void drawSprite(uint32_t texture, glm::vec2 pos, glm::vec2 size, float rotation, float alpha = 1.0f);
private:
    SDL_Window *window;
    SDL_GLContext glcontext;
    uint32_t sprite_shader, quad_vao;
};

}
}
