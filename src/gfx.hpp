#pragma once

#include <SDL3/SDL_video.h>
#include <SDL3/SDL_render.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include "shader.hpp"
#include "gfx_textures.hpp"

namespace rana {
namespace gfx {

class Context {
    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using vec4 = glm::vec4;

    SDL_Window *window;
    SDL_GLContext glcontext;
    Shader sprite_shader;
    uint32_t quad_vao;

public:
    Context(const char *title = "rana", int w = 1280, int h = 720);
    ~Context();

    void drawBegin();
    void drawFinish();

    void clear(vec3 color);
    void drawSprite(Texture &tex, vec2 pos, vec2 scale = {1,1}, float r = 0, vec4 color = {1,1,1,1});
};

}
}
